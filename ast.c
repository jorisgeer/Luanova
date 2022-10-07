/* ast.c - create and process abstract syntax tree

   This file is part of Luanova, a fresh implementation of Lua.

   Copyright © 2022 Joris van der Geer.

   Luanova is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Luanova is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program, typically in the file License.txt
   If not, see http://www.gnu.org/licenses.
 */

/* An AST is generated from the info the parser provides.
   This AST is then processed in multiple passes :
   - applying expression precedence
   - var binding
   - intermediate code generation
 */

#include <math.h>
#include <string.h>

#include "base.h"

#ifdef __clang__
 #pragma clang diagnostic warning "-Wduplicate-enum"
#endif

#include "mem.h"

#include "chr.h"

#include "fmt.h"

static ub4 msgfile = Shsrc_ast;
#include "msg.h"

#include "util.h"

#include "lsa.h"

#include "astyp.h"
#include "synast.h"

#include "ast.h"

#define Depth 256 // block nesting

static char atynames[] =
  "id var ilit flit slit ilits slits "
  "tru fal kwd op "
  "subscr fstr "
  "pexp uexp bexp aexp grpexp "
  "blk "
  "if while "
  "fndef param param_id "
  "atgt_id asgnst stmt fstring "
  "pexplst fstrlst prmlst stmtlst "
  "eof "
  "count *inv*";

static ub2 atynpos[Acount+2];

static char uopnam[] = "+-~!.";

static cchar *bopnam[] = { "or","and","!=","==","<<",">>","^","|","&","+","-","*","/","%","?","^","<",">","<=",">=","ocount","*inv*" };

static void atyinit(void) {
  char *ap = atynames;
  ub2 t=0;
  ub2 pos=0;

  while (ap[pos] && t <= Acount) {
    atynpos[t] = pos;
    while (ap[pos] != ' ') pos++;
    ap[pos++] = 0; t++;
  }
}

void iniast(void) {
  atyinit();
}

cchar *atynam(enum Astyp t)
{
  t = min(t,Acount+1);
  return atynames + atynpos[t];
}

#define Expdep 16 // max expression precedence depth

// binop precedences
static ub1 oprecs[Obcnt] = {
  [Omul]  = 12, [Odiv]    = 12, [Omod]   = 12, [Oadd]  = 11, [Osub]  = 11,
  [Oshl]  = 10, [Oshr]    = 10, [Olt]    = 9,  [Ogt]   = 9,  [Ole]   = 9,
  [Oge]   = 9,  [One]     = 8,  [Oeq]    = 7,  [Oand]  = 6,  [Oxor]  = 5,
  [Oor]   = 4,  [Oreland] = 3,  [Orelor] = 2 };

#define Obbit 27
#define Regbit 23
#define Regmsk 0x7fffff
#define Obmsk  0x7ffffff

static enum Bop lx2bop(ub2 c) {
  if (c & 0x200) {
    switch(c & 0xff) {
    case '<': return Oshl;
    case '>': return Oshr;
    case '*': return Oexp;
    case '/': return ODiv;
    }
  } else if (c & 0x100) {
    switch(c & 0xff) {
    case '<': return Ole;
    case '>': return Oge;
    case '=': return Oeq;
    case '!': return One;
    }
  } else {
    switch(c) {
    case '<': return Olt;
    case '>': return Ogt;
    case '+': return Oadd;
    case '-': return Osub;
    case '*': return Omul;
    case '/': return Odiv;
    case '%': return Omod;
    case '@': return Omxm;
    case '&': return Oand;
    case '|': return Oor;
    case '^': return Oxor;
    }
  }
  return Oeq;
}

// create full expr from list of half 'unary-expr+operator' using shunting yard precedence
// todo count nlit binop nlit to alloc spare nodes for eval
static ub2 precexp(struct ast *ap,struct rexp *rxp,ub4 *ops,ub4 pos,ub2 n)
{
  ub2 i;
  ub2 opp,ost[Expdep];
  ub2 vst[Expdep];
  ub2 vi=0,osp=0,vsp=0;
  ub2 reg=0,r1,r2,hit=0;
  ub4 ei;
  ub4 pnn,nh,en;
  enum Astyp et;
  enum Bop op,op2;
  struct pexp *pe,*pes = ap->pexps;
  ub8 repid;
  const ub8 * restrict repool = ap->repool;
  ub4 *nhs = ap->nhs;

  *ost = 0;

  for (i = 0; i < n-1; i++) {
    repid = repool[pos];
    pos = repid >> 32;
    en = repid & hi32;
    nh = nhs[en];
    et = nh >> Atybit; // pexp
    ei = nh & Atymsk;
    pe = pes + ei;
    op = pe->op;
    // info("op %s",bopnam[op]);

    reg = vsp;
    vst[vsp++] = reg | (i << 8);
    if (reg > hit) hit = reg;
    ops[vi++]  = pe->e | ((ub4)reg << Regbit) | ((ub4)Obcnt << Obbit); // eval req  4+4+24

    opp = (ub2)oprecs[op] << 5;
    while (opp <= ost[osp]) {
      op2 = ost[osp--];
      // info("op %u prec %u",op2 & 0x1f, op >> 5);
      r1 = vst[vsp-2] & 0xff;
      r2 = vst[vsp-1] & 0xff;
      ops[vi++] = r1 | ((ub4)r2 << Regbit) | ((ub4)op2 << Obbit); // r0 = r1 op r2 4+4+4
      vst[vsp-2] = 0xff00; // r0
      vsp--;
    }

    if (osp >= Expdep) serror(0,"exp stk exceeds %u",Expdep);
    ost[++osp] = op | opp;
  }

  // last item without op
  repid = repool[pos];
  en = repid & hi32;
  nh = nhs[en];
  et = nh >> Atybit; // pexp
  ei = nh & Atymsk;
  pe = pes + ei;
  op = pe->op;

  reg = vsp;
  vst[vsp++] = reg | (i << 8);
  ops[vi++]  = pe->e | ((ub4)reg << Regbit) | ((ub4)Obcnt << Obbit);

  while (osp) {
    op2 = ost[osp--];
    // info("op %u prec %u",op2 & 0x1f, op >> 5);
    r1 = vst[vsp-2] & 0xff;
    r2 = vst[vsp-1] & 0xff;
    ops[vi++] = r1 | ((ub4)r2 << Regbit) | ((ub4)op2 << Obbit);
    vst[vsp-2] = 0xff00;
    vsp--;
  }

  rxp->hit = hit;
  info("vi %u cnt %u",vi,n);
  return vi;
}

static enum Bop lx2aop(ub1 c)
{
  return lx2bop(c & 0x1ff);
}

static enum Uop lx2uop(ub1 c)
{
  enum Uop o;

  switch (c) {
  case '*': o = Oumin; break;
  case '~': o = Oneg;  break;
  case '!': o = Onot;  break;
  case '+': o = Oupls; break;
  case '-': o = Oumin; break;
  default: o = Oucnt;
  }
  return o;
}

#if 0
static cchar *tknam(enum token tk)
{
  ub2 len;
  ub1 hc;
  char *p,*q;
  static char basbuf[8 * 32];
  char *buf;
  static ub2 bufno;

  buf = basbuf + bufno * 32;
  bufno = (bufno + 1) & 7;

  if (tk >= t99_count) return "Tinv";
#if Kwcnt > 0
  memcpy(buf,tkwnampool + tkwnamposs[tk],len=tkwnamlens[tk]);
#endif
  buf[len] = 0;
  buf[len+1] = ' ';
  return buf;
}
#endif

static const ub1 *slits_str(ub4 x,ub4 *plen,const ub1 *slitpool)
{
  static ub1 buf[4];
  ub4 pos,len = x >> Atrbit;

  *plen = len;
  switch (len) {
  case 0: *plen = 0; return (const ub1 *)"";
  case 1: buf[0] = x & 0xf; break;
  case 2: buf[0] = x & 0xf; buf[1] = (x >> 8) & 0xf; break;
  case 3: return slitpool + (x & Atrmsk); break;
  default: pos = slitstr(x & Atrmsk,&len); *plen = len;
           return slitpool + pos;
  }
  return buf;
}

static ub1 *slithrbuf;

static ub4 hrslit(const ub1 *s,ub4 len,ub4 fpx)
{
  ub4 xat = fpx >> Lxabit;
  char q;
  ub1 *d = slithrbuf;
  ub4 nq,n = 0;

  if (fpx & (1U << Slitqbit)) q = '"'; else q = '\'';
  if (fpx & (1U << Slitlbit)) nq = 3; else nq = 1;

  if (xat & Slitctl_f) d[n++] = 'f';
  if (xat & Slitctl_r) d[n++] = 'r';
  if (xat & Slitctl_b) d[n++] = 'b';
  d[n++] = q;
  if (nq == 3) { d[n++] = q; d[n++] = q; }

  n += chprintn(s,len,d+n,xat);
  d[n++] = q;
  if (nq == 3) { d[n++] = q; d[n++] = q; }
  d[n] = 0;
  return n;
}

// todo placeholders
static ub4 mkop(ub1 flvl,ub4 x,ub4 y)
{ return 0;
}

static ub4 mklb(ub1 flvl,ub4 x)
{return 0;
}

static ub4 setlb(ub1 flvl,ub4 x)
{return 0;
}

static ub4 beval(ub4 ival1,enum Bop op,ub4 ival2) { return ival1 * ival2; }

#if 0
static void mkfnprms(struct fndef *fp,ub4 fn,ub4 pln)
{
  struct stmtlst *slp;
  ub4 pos = slp->pos;
  ub2 cnt = slp->cnt;
  ub4 ss,sn,si;
  enum Astyp st;

  if (cnt < 2) ice(0,0,"stmt list len %u",cnt);
  cnt--;
  nn = repool[pos+cnt];
  do {
    cnt--;
    ss = repool[pos+cnt];
    sn = nhs[ss];
    st = sn >> Atybit;
    si = sn & Atymsk;
    switch (st) {
//      case Aid: idp = ids + si; idp->rol = Fprm; break;
      case Ailit: case Ailits:
      case Aflit:
      case Aslit:
      case Akwd: break;
      default:
    }
  } while (cnt);

}
#endif

struct varscope {
  ub4 v0;
  ub2 n;
};

#define psh(n,p) stk[sp++] = n | (p << 28)
#define pop(n,p) sp--; n = stk[sp] & hi28; p = stk[sp] >> 28

static void process(struct ast *ap,bool emit)
{
  cchar *name = ap->name;

  ub4 sp=0;

  ub4 nh,*nhs = ap->nhs;
  ub4 fpx,fps,*restrict fpos = ap->fpos;

  struct agen *gp,*gs = ap->gens;

  struct aid *idp, *ids  = ap->ids;
  struct var *varp,*vars = ap->vars;

  struct ilit *ilitp,*ilits = ap->ilits;
  struct flit *flitp,*flits = ap->flits;
  struct slit *slitp,*slits = ap->slits;

  struct pexp *pexpp,*pexps = ap->pexps;
  struct uexp *uexpp,*uexps = ap->uexps;
  struct bexp *bexpp,*bexps = ap->bexps;
  struct aexp *aexpp,*aexps = ap->aexps;

  struct asgnst *asgnstp,*asgnsts = ap->asgnsts;

  struct blk *blkp,*blks = ap->blks;

  struct aif   *ifp,   *ifs    = ap->ifs;
  struct witer *witerp,*witers = ap->witers;

  struct fndef *fndefp,*fndefs = ap->fndefs;
  struct param *prmp,*prms = ap->prms;

  struct stmt *stmtp,*stmts = ap->stmts;

  struct rexp *rexpp,*rexps = ap->rexps;
  struct prmlst *prmlp,*prmls = ap->prmls;
  struct stmtlst *stmtlp,*stmtls = ap->stmtls;

  ub4 witercnt = ap->ndcnts[Awhile];

  ub4 nid = ap->nid;
  ub4 aidcnt = ap->aidcnt;

  ub4 uid,uidcnt = ap->uidcnt;

  ub4 vid = 0,parvid,vid0,curvid = 0,vidf0=0,varcnt = nid;

  ub2 hiblklvl = ap->hiblklvl;

  const ub1 *slitpool = ap->slitpool;

  ub4 enn,lnn,rnn,tnn,ss,en;
  ub4 ne,enh,lnh,rnh,tnh,plnh,sn;
  ub4 nn,ni,ei,ni2,nidef,blkni,lni,rni,li,ri,tni,plni,si;
  enum Astyp t=0,tt,et,lt,rt,st,nt,prvt=Acount;
  ub4 tgt,ret,tru,fal;

  ub4 prmlst,nnd=0,ndcnt = ap->len;
  ub4 n;
  ub4 lpos,lcnt,pos=0,nxt;
  ub4 scid;
  ub4 ofs;
  ub4 id;
  ub2 lvl=0,vl,lvv;
  ub2 loplvl=0;
  ub2 fnlvl=0;

  ub8 repid;
  const ub8 * restrict repool = ap->repool;
  ub4 replen = ap->replen;

  ub4 *ops,*opspool = ap->opspool;
  ub4 opos;
  ub2 ocnt;

  enum Typ ty,lty,rty;
  ub2 fac,r,oi;
  ub4 cnt=0,cur;

  ub4 ival,ival1=0,ival2=0;
  ub4 x4;
  double fval,fval1,fval2;
  enum Bop op;
  enum Uop uop;

  ub4 head,pc0,pc = 0;
  ub1 reg,reg1,breg,res;

  ub4 qlen = (Depth * 8) + Nodarg;
  ub4 *stk = minalloc(qlen + 8,4,Mnofil,"ast q");
  ub1 pas;

//  ub1 blkbit = ap->blkbit;
  ub4 lvlen = hiblklvl + 2;
  ub8 lvlmsk,*lvlvars = alloc(uidcnt,ub8,Mnofil,"ast lvlvars",nextcnt); // idid
  ub4 *var2uid = alloc(varcnt,ub4,Mnofil,"ast var2uid", nextcnt);

  ub4 *idvars  = alloc(uidcnt * lvlen,ub4,Mnofil,"ast idvars", nextcnt);

  struct varscope varscs[Depth];
  struct varscope *vlp;
  ub2 blkbit=1;
  ub4 vp,vp0,nxv0;

  memset(varscs,0,sizeof(varscs));

  ub4 iter=0;

  bool pretty = globs.emit >> 15;
  char ppnam[512];
  char buf[256];
  struct bufile fp;
  ub4 len;
  cchar *namsp;
  const ub1 *str;

  if (pretty) {
    namsp = strrchr(name,'.');
    if (namsp) len = namsp - name;
    else { len = strlen(name); namsp = ".py"; }
    mysnprintf(ppnam,0,512,"%.*s.pretty%s",len,name,namsp);
    fp.nam = ppnam;
    fp.dobck = 1;

    myfopen(FLN,&fp,16384,1);
    info("writing %s",ppnam);
    msgfls();
  }

  // todo separate binding sweep with imports
  // first bind mod, then import to root

  psh(ap->root,0);

  while (sp) {
    pop(nn,pas);

// -----
    next:
// -----
    iter++;
    if (iter > 10 * aidcnt) ice(0,0,"iter %u exceeds 10 aid %u",iter,aidcnt);
    if (sp >= qlen) ice(0,0,"sp %u overflow at iter %u",sp,iter);

    // info("sp %u node %u/%u",sp,nn,aidcnt);

    if (nn >= aidcnt) ice(0,0,"sp %u node %u.%x/%u prv %s",sp,nn,nn,aidcnt,atynam(t));
    nh = nhs[nn];
    fpx = fpos[nn];
    fps = fpx & Lxamsk;
    t = nh >> Atybit;
    ni = nh & Atymsk;
    gp = gs + nn;

    if (iter < 200) sinfo(fps,"sp %3u nd %3u ni %3u pas %u %-6s",sp,nn,ni,pas,atynam(t));

    switch(t) {

    // leaves
    case Avar: break;

    case Aid:
      idp = ids + ni;
      idp->lvl = lvl;
      id = idp->id;
      if (iter < 200) sinfo(fps,"id %u %u %s",ni,id,idnam(id));
      if (id == hi32) ice(fps,0,"nid %u.%u typ id no val",nn,ni);
      if (pretty) myfputs(&fp,(cchar *)idnam(id));
    break;

    case Ailit:
      ilitp = ilits + ni;
      if (pretty) myfputs(&fp,ultoa(buf+64,ilitp->val));
      info("ilit %u %lu",ni,ilitp->val);
    break;

    case Ailits:
      if (pretty) myfputs(&fp,utoa(buf+64,ni));
      info("ilits %u",ni);
    break;

    case Aflit:
      flitp = flits + ni;
      info("flit %u",ni);
    break;

    case Aslit:
      slitp = slits + ni;
      if (pretty) {
        str = slitpool + slitstr(slitp->val,&len);
        len = hrslit(str,len,fpx);
        myfwrite(&fp,slithrbuf,len);
      }
      info("slit %u %u",ni,slitp->val);
    break;

    case Aslits:
      if (pretty) {
        str = slits_str(ni,&len,slitpool);
        len = hrslit(str,len,fpx);
        myfwrite(&fp,slithrbuf,len);
      }
      info("slits %x",ni);
    break;

    case Atru:
    case Afal: break;

    case Akwd: break;

    case Aop: op = ni;
    break;

    // expressions
    case Asubscr:
    break;

    case Agrpexp:
    break;

    case Auexp:
      uexpp = uexps + ni;
      enn = uexpp->e;
      enh = nhs[enn];
      et = enh >> Atybit;
      ei = enh & Atymsk;
      uop = uexpp->op;
      if (pas == 0) { // bind var
        // info("uexp %u %u",enn,uop);
        if (pretty) {
          if (uop != Oucnt) myfputc(&fp,uopnam[uop]);
        }
        switch (et) {
          case Aid: // todo bind var
            break;
          case Ailit: gp->ty = Yint; break;
          case Aflit: gp->ty = Yflt; break;
          default:    break;
          }
        psh(nn,1);
        nn = enn;
        if (nn == hi32) warning("uexp %u no e",ni);
        else goto next;
      } else {
        gp->ty = gs[enn].ty; break;
      }
    break;

    case Apexp: // todo replace by uexp
      pexpp = pexps + ni;
      enn = pexpp->e;
      enh = nhs[enn];
      et = enh >> Atybit;
      ei = enh & Atymsk;

      if (pas == 0) {
        psh(nn,1);
        nn = enn;
        if (nn == hi32) warning("uexp %u no e",ni);
        else goto next;
      } else {
      }
    break;

    case Abexp:
      bexpp = bexps + ni;

      rnn = bexpp->r;
      lnn = bexpp->l;
      op  = bexpp->op;
      rnh = nhs[rnn];
      lnh = nhs[lnn];
      rt  = rnh >> Atybit;
      lt  = lnh >> Atybit;
      ri  = rnh & Atymsk;
      li  = lnh & Atymsk;
      breg = gp->res;

      if (pas == 0) {
        if (op != 0xff && op > Obcnt) ice(fps,0,"bexp %u.%u invalid op %x",nn,ni,op);
        if (pretty) myfprintf(&fp," as %s in ",op == 0xff ? "=" : bopnam[op]);

#if 0
        if (lt > Aval && rt > Aval) {
          psh(nn,1);
          psh(rnn,0);
          gs[lnn].res = breg;
          gs[rnn].res = breg+1;
          nn = lnn; goto next;
        }

        lty = rty = 0;
        switch (lt) {
          case Aid: // todo bind var
            break;
          case Ailit:
            lty = Yint;
            ival1 = ilits[li].val;
            mkop(0,0,ival1); // ld r0
            break;
          case Ailits:
            lty = Yint;
            ival1 = li;
            break;
          case Aflit:
            lty = Yflt;
            fval1 = flits[li].val;
            break;
          case Avar:
          case Auexp:
            psh(nn,1);
            gs[lnn].res = breg;
            nn = lnn; goto next;
          default: break;
        }
        gp->ty = lty;

        switch (rt) {
          case Aid: // todo bind var
            break;
          case Ailit:
            rty = Yint;
            ival2 = ilits[ri].val;
            mkop(0,0,ival2); // ld r0
            break;
          case Ailits:
            rty = Yint;
            ival2 = ri;
            break;
          case Aflit:
            rty = Yflt;
            fval2 = flits[ri].val;
            break;
          case Avar:
          case Auexp:
            psh(nn,1);
            gs[rnn].res = breg;
            nn = rnn; goto next;
          default: break;
        }
        gp->ty = rty;

        if (lty && rty) { // 1 + 1
          lni = lnh & Atymsk;
          rni = lnh & Atymsk;
          if (lty == Yint) {
             if (rty == Yint) {
               nhs[nn] = lnn;
               ilits[lni].val = beval(ival1,bexpp->op,ival2);
             }
           }
        }
#endif

      } else { // pas 1

#if 0
        switch (lt) {
          case Auexp:
            gp->ty = gs[lnn].ty;
            break;
          default: break;
        }

        switch (rt) {
          case Auexp: gp->ty = gs[lnn].ty; break;
          default: break;
        }
#endif

      }
    break;

    case Aaexp: // a := b todo
      info("aexp %u",ni);
      aexpp = aexps + ni;

      enn = aexpp->e;
      // info("aexp %u",enn);
      if (enn != hi32) {
        enh = nhs[enn];
        // info("aexp %u",enh);
        et = enh >> Atybit;
        ei = enh & Atymsk;
        // info("aexp %u",ei);
      }

      lnn = aexpp->id;

      if (pas == 0) {
        if (pretty) myfputs(&fp," [ ");
        psh(nn,1);
        nn = lnn;
        if (nn == hi32) info("aexp %u no e",ni);
        else goto next;
      } else {
        if (pretty) myfputs(&fp," ] ");
      }
    break;

    case Afstr:
    case Afstring:
    break;

    case Atgt_id: break;

    // statements
    case Aasgnst: // a[i] = 3
      asgnstp = asgnsts + ni;
      enn = asgnstp->e;
      if (pas == 0) {

        tnn = asgnstp->tgt;
        tnh = nhs[tnn];
        tt  = tnh >> Atybit;
        tni = tnh & Atymsk;

        gs[tnn].ty = gs[enn].ty;

        if (tt == Aid) { // common a =
          idp = ids + tni;
          uid = idp->id;
          lvlmsk = lvlvars[uid];
          lvv = lvl;
          while ( (lvlmsk & (1U << lvv)) == 0) lvv--;
          if (lvv) {
            vid = idvars[lvv * uid];
            varp = vars + vid;
          } else {
            lvv = lvl;
            vid = curvid++;
            lvlvars[uid] |= (1U << lvv);
            var2uid[vid] = vid;
            varp = vars + vid;
            varp->id = tni;
            varp->ofs = vid - vidf0;
            sinfo(fpos[tni] & Lxamsk,"new var %u@%u lvl %u",uid,vid,lvl);
          }
          varp->lvl = lvv;
          var2uid[vid] = uid;
          nhs[tnn] = (Avar << Atybit) | vid;
        }

        enh = nhs[enn];
        et  = enh >> Atybit;
        ei  = enh & Atymsk;

        switch (et) { // rhs expr
          case Aid:
            idp = ids + ei;
            uid = idp->id;
            lvlmsk = lvlvars[uid];
            lvv = lvl;
            while ( (lvlmsk & (1U << lvv)) == 0) lvv--;
            if (lvv == 0) serror(fpos[ei] & Lxamsk,"unknown var %u at lvl %u",uid,lvl);
            vid = idvars[lvv * uid];

            break;
          case Ailit: gp->ty = Yint; break;
          case Aflit: gp->ty = Yflt; break;
          case Avar: break;
          default:
            psh(nn,1);
            gs[enn].res = 0;
            nn = enn; goto next; // exp
        }

      } else if (pas == 1) { // after rhs in r0
        tnn = asgnstp->tgt;
        tnh = nhs[tnn];
        tt  = tnh >> Atybit;
        tni = tnh & Atymsk;

        gs[tnn].ty = gs[enn].ty;

        if (tt == Avar) { // common a = todo bind
          ofs = vars[tni].ofs;
          mkop(0,0,ofs);
        } else {
          nn = tnn; pas = 2; goto next; // a[i] =
        }

      } else if (pas == 2) { // after tgt
      }
    break;

    case Ablk:
      blkp = blks + ni;
      if (pas == 0) {
        // info("blk %u sp %u",ni,sp);
        if (pretty) myfputs(&fp," { ");

        psh(nn,1);
        varscs[++lvl].v0 = vid;
        blkbit <<= 1;
        nn = blkp->s;
        if (nn >= aidcnt) warning("blk %u s %x",ni,nn);
        else goto next;

      } else { // clear out-of-scope vars
        if (pretty) myfputs(&fp," } ");

        vlp = varscs + lvl;
        vp0 = vlp->v0;
        nxv0 = vlp[1].v0;
        if (nxv0 != hi32) {
          for (vp = vp0; vp < nxv0; vp++) {
            uid = var2uid[vp];
            lvlvars[uid] &= ~blkbit;
          }
          for (vp = nxv0 + vlp[1].n; vp < vp0 + vlp->n; vp++) {
            uid = var2uid[vp];
            lvlvars[uid] &= ~blkbit;
          }
        } else {
          for (vp = vp0; vp < vid; vp++) {
            uid = var2uid[vp];
            lvlvars[uid] &= ~blkbit;
          }
        }
        lvl--;
        blkbit >>= 1;
      }
    break;

    case Aif:
      ifp = ifs + ni;
      if (pas == 0) {
        info("if %u",ni);
        if (pretty) myfputs(&fp,"if (");
        psh(nn,1);
        nn = ifp->e;
        goto next;
      } else if (pas == 1) {
        if (pretty) myfputs(&fp,") ");
        psh(nn,2);
        nn = ifp->tb;
        pas = 0; goto next;
      } else if (pas == 2) {
        // if (pretty) myfputs(&fp," }");
      }
    break;

/* while (expr) trustmt else falstmt
1 :head
2  a = expr()
3  bne a, :false
4 :true
5  stmts
6  jmp :head
7 :false
8  stmts
9 :end   */
    case Awhile:
      witerp = witers + ni;
      breg = gp->res;
      if (pas == 0) {
        witerp->lvl = loplvl++;
        witerp->head = mklb(pc,0); // 1
        enn = witerp->e;
        enh = nhs[enn];
        et  = enh >> Atybit;
        ei  = enh & Atymsk;

        switch (et) {
        case Atru: pas = 2; goto next;
        case Afal: pas = 3; goto next;
        case Avar:
          // mkldop(reg,vars[ei].ofs);
          pas = 1; goto next;
        case Ailit: if (ilits[ei].val) pas = 2; else pas = 3; goto next;
        case Auexp:
        case Abexp:
        case Apexplst:
          psh(nn,1);
          nn = enn; gs[enn].res = breg; goto next;
        default: break;
        }

      } else if (pas == 1) { // expr

        witerp->bcc = mkop(0,breg,0); // 3, bnz patch after tru stmt
        mklb(pc,ni); // 4

        psh(nn,2);
        tru = witerp->tb;
        nn = tru; pas = 0; goto next; // 5

      } else if (pas == 2) { // tru body
        head = witerp->head;
        mkop(0,head,0); // 6 jmp
        setlb(witerp->bcc,pc);
        loplvl--;
        fal = witerp->fb;
        if (fal != hi32) {
          mklb(0,0);
          psh(nn,3); nn = fal; pas = 0; goto next;
        } else {
          witerp->tail = pc;
          break;
        }

      } else if (pas == 3) { // fal body
        witerp->tail = pc;
      }
    break;

    case Aparam_id: break;

    case Aparam:
      prmp = prms + ni;
      if (pas == 0) {
        if (pretty) myfputs(&fp," ( ");
        psh(nn,1);
        nn = prmp->id;
        if (iter < 10) info("param %u id %u",ni,nn);
        if (nn == hi32) warning("param %u no e",ni);
        else goto next;
      } else {
        if (pretty) myfputs(&fp," ) ");
      }
    break;

    case Afndef:
      fndefp = fndefs + ni;
      if (pas == 0) {
        psh(nn,1);
        fndefp->parfn = ni;
        fndefp->parvid = vidf0;
        vidf0 = fndefp->vid0 = curvid;
        fndefp->pc0 = pc;
        fnlvl++;
        prmlst = fndefp->plst;
        idp = ids + (fndefp->id & Atymsk);
        id = idp->id;
        ret = fndefp->ret;
        if (prmlst != hi32) {
          plnh = nhs[prmlst];
          plni = plnh & Atymsk;
          prmlp = prmls + plni;
          pos = prmlp->pos;
          cnt = prmlp->cnt;
          fndefp->argc = cnt;
          fac = 0;
          if (cnt < 2) ice(0,0,"type %u stmt list %u",t,cnt);
          do {
            ss = repool[pos+fac];
            sn = nhs[ss];
            st = sn >> Atybit;
            si = sn & Atymsk;
            switch (st) {
            case Aid: // todo bind
              idp = ids + si;
              varp = vars + curvid;
              nhs[ss] = curvid | (Avar << Atybit);
              info("new fn par %u at lvl %u",curvid,lvl);
              break;
            default: break;
            }
          } while (fac < cnt);
        }
        blkni = fndefp->blk;
        nn = blkni;
        goto next;
      } else if (pas == 1) {
        fndefp->vidcnt = curvid - fndefp->vid0;
        fndefp->pc1 = pc;
        fnlvl--;
        fndefp = fndefs + fndefp->parfn;
        vidf0 = fndefp->vidf0;
      }
    break;

    case Astmt:
      stmtp = stmts + ni;
      if (pas == 0) {
        psh(nn,1);
        nn = stmtp->s;
        if (nn == hi32) warning("stmt %u no s",ni);
        else goto next;
      } else if (pas == 1) {
        if (pretty) myfputc(&fp,';');
      }
      // info("ni %u / %u",ni,ap->ndcnts[Astmt]);
    break;

    case Apexplst:
      rexpp = rexps + ni;
      cnt = rexpp->ocnt;
      oi = rexpp->opndx;
      info("oi %u/%u",oi,cnt);

      pos = rexpp->pos;
      opos = rexpp->opos;
      ops = opspool + opos;

      breg = gp->res;

      x4 = ops[oi++];
      op = x4 >> Obbit;
      reg = (x4 >> Regbit) & Regmsk;
      if (oi < cnt) psh(nn,0);
      rexpp->opndx = oi;
      if (op == Obcnt) { // eval req
        en = x4 & Regmsk;
        // info("nn %u",en);
        nn = en;
        gs[en].res = reg + breg;
        goto next;
      } else if (op < Obcnt) {
        // info("op %u",op);
        reg1 = x4 & Regmsk;
        if (pretty) myfprintf(&fp," %s ",bopnam[op]);
        mkop(op,reg1,reg); // r0 = r1 op r2
      } else info("op %u",op);

    break;

    // lists
    case Aparamlst: // handled at fndef ?
    break;

    case Astmtlst:
      stmtlp = stmtls + ni;
      cnt = stmtlp->cnt;

      pos = stmtlp->pos;

      if (cnt < 2) ice(0,0,"stmtlst ni %u cnt %u",ni,cnt);

      if (pas == 0) {
        psh(nn,1);
        repid = repool[pos];
        nn = repid & hi32;
        nxt = repid >> 32;
        info("rep %u nxt %u sp %u nn %u",pos,nxt,sp,nn);
        if (nxt >= replen) ice(0,0,"stmtlst repos %u above %u",nxt,replen);
        stmtlp->pos = nxt;
        goto next;
      } else if (pas == 1) {
        if (pretty) myfputc(&fp,';');
        if (pos) { pas = 0; goto next; }
      }
      break;

    case Afstrlst:
    break;

    case Acount: ice(fps,0,"invalid node type for %u iter %u sp %u",nn,iter,sp);

    default: ice(fps,0,"invalid node type for %u iter %u sp %u",nn,iter,sp);

    } // switch (typ)

  } // while (sp)

end:

  afree(stk,"ast q",nextcnt);

  if (pretty) {
    myfputc(&fp,'\n');
    myfclose(&fp);
    info("wrote %s",ppnam);
    msgfls();
  }

}

void *mkast(struct synast *sa)
{
  ub8 T0=0,T1;

  timeit(&T0,nil);
  T1 = T0;
  ub4 t1 = (ub4)(T1 / 1000);

  struct ast *ap = minalloc(sizeof(struct ast),0,0,"ast");

  ub4 *restrict nhs = sa->nhs;
  ub4 *restrict nfps = sa->fps;
  ub8 *restrict args = sa->args;
  ub2 *nlnos = sa->nlnos;

  ub4 acnt2,argcnt = sa->argcnt;
  ub8 val=0,*restrict vals = sa->vals;
  ub4 vi=0,valcnt = sa->valcnt;

  ub4 fps=0,fpx=0;
  ub1 xat=0;
  ub4 ti;

  ub4 nvar,nuex,nbex;
  ub4 repos=0;
  ub4 a=0,a0=0,a1=0,a2=0,a3=0,lfa,ni,ni0,ii,i0,ai,ani;
  ub2 lf,nlf;
  ub4 fpos_ilit=0;
  ub2 dfp;
  ub4 bits=0;
  ub2 atr;

  cchar *name = sa->name;
  const ub1 *str;
  char buf[64];

  ub2 am,ac,la;

  ub4 rni = 0;
  ub4 gi,lnn,rnn;
  ub4 nh,a0h=0,a1h,lnh,rnh;
  ub4 an,pn,pn1,an1,an2,an3;
  ub4 anh,anh1,anh2,anh3;
  ub4 pos;
  ub4 pni,ni1,ni2,ni3;
  ub4 li,ri;
  enum Astyp t,t0,lt,rt,at,at1,at2,at3;
  enum Bop bop;
  enum Uop uop;

  ub4 cnt=0,cnt2=0,len;

  ub4 fxs;
  int fxp;
  ub4 ival1,ival2;
  double fval;

  bool emit = (globs.emit & Astpas);

  sassert(Acount <= Atymsk,"atypes");

  ub4 aidcnt = sa->aidcnt;
  ub4 uidcnt = sa->uidcnt;
  ub4 idcnt  = sa->idcnt;

  const ub1 *slitpool = sa->slitpool;
  ub4 slithilen = sa->slithilen;

  ub2 lvl,blklvl=0,prvblklvl=0,hiblklvl = 0;

  ub4 *ndcnts = sa->ndcnts;
  ub4 *rep2cnts = sa->rep2cnts;
  ub4 repcnt = sa->repcnt;

  ub8 *repool = sa->repool;

  ub4 nid = ndcnts[Aid];

  if (nid != idcnt) swarn(0,"idcnt %u vs %u dif %u",idcnt,nid,nid - idcnt);

  showcnt("id",idcnt);
  showcnt("uid",uidcnt);

  ub4 nilit = ndcnts[Ailit];
  ub4 nflit = ndcnts[Aflit];

  ub4 nslit = ndcnts[Aslit] + ndcnts[Aslits];

  ub4 nop = ndcnts[Aop];

  ub4 nuexp = ndcnts[Auexp];
  ub4 naexp = ndcnts[Aaexp];

  ub4 npexp = ndcnts[Apexp];

  ub4 nrexp = ndcnts[Apexplst];
  ub4 nbexp = rep2cnts[Apexplst];

  ub4 nif    = ndcnts[Aif];
  ub4 nwiter = ndcnts[Awhile];

  ub4 nblk  = ndcnts[Ablk];

  ub4 nasgnst = ndcnts[Aasgnst];

  ub4 nprm = ndcnts[Aparam];

  ub4 nstmt = ndcnts[Astmt];

  ub4 nstmtlst = ndcnts[Astmtlst];
  ub4 histlstsiz = 0;

  struct mempart ndpart[Acount];

  memset(ndpart,0,sizeof ndpart);

  for (t = 0; t <= Acount; t++) {
    cnt = ndcnts[t];
    cnt2 += cnt;
    showscnt(6,atynam(t),cnt);
    switch (t) {
    case Ailits:
    case Aslits:
    case Aop: cnt = 0; break;
    default: break;
    }
    ndpart[t].nel = cnt;
  }
  if (aidcnt > cnt2) warning("aidcnt %u vs %u dif %u",aidcnt,cnt2,aidcnt - cnt2);
  else if (aidcnt < cnt2) ice(0,0,"aidcnt %u vs %u dif %u",aidcnt,cnt2,aidcnt - cnt2);

  ndpart[Avar].nel = nid;

  struct agen *genp,*gens = alloc(aidcnt,struct agen,Mnofil,"ast gen",nextcnt);

  ndpart[Aid].siz = sizeof(struct aid);
  ndpart[Avar].siz = sizeof(struct var);

  ndpart[Ailit].siz = sizeof(struct ilit);
  ndpart[Aflit].siz = sizeof(struct flit);
  ndpart[Aslit].siz = sizeof(struct slit);

  ndpart[Auexp].siz = sizeof(struct uexp);
  ndpart[Apexp].siz = sizeof(struct pexp);
  ndpart[Aaexp].siz = sizeof(struct aexp);

  ndpart[Aasgnst].siz = sizeof(struct asgnst);

  ndpart[Ablk].siz = sizeof(struct blk);

  ndpart[Aif].siz    = sizeof(struct aif);
  ndpart[Awhile].siz = sizeof(struct witer);

  ndpart[Afndef].siz = sizeof(struct fndef);
  ndpart[Aparam].siz = sizeof(struct param);

  ndpart[Astmt].siz = sizeof(struct stmt);

  ndpart[Apexplst].siz = sizeof(struct rexp);
  ndpart[Aparamlst].siz = sizeof(struct prmlst);
  ndpart[Astmtlst].siz = sizeof(struct stmtlst);

  for (t = 0; t < Acount; t++) {
    ndpart[t].fil = Mnofil;
    ndpart[t].dsc = atynam(t);
  }

  void *ndbas = allocset(ndpart,Acount,/*Mnofil*/0xff,"ast tree",nextcnt);

  struct aid *idp,*ids = ndpart[Aid].ptr;
  struct var *varp,*vars = ndpart[Avar].ptr;

  struct ilit *ilitp,*ilits = ndpart[Ailit].ptr;
  struct flit *flitp,*flits = ndpart[Aflit].ptr;
  struct slit *slitp,*slits = ndpart[Aslit].ptr;

  struct uexp *uexpp,*uexps = ndpart[Auexp].ptr;
  struct pexp *pexpp,*pexps = ndpart[Apexp].ptr;
  struct aexp *aexpp,*aexps = ndpart[Aaexp].ptr;

  struct bexp *bexpp,*bexps = ndpart[Abexp].ptr; // todo tmp

  struct asgnst *asgnstp,*asgnsts = ndpart[Aasgnst].ptr;

  struct blk *blkp,*blks = ndpart[Ablk].ptr;

  struct aif *ifp,*ifs = ndpart[Aif].ptr;

  struct witer *witerp,*witers = ndpart[Awhile].ptr;

  struct fndef *fndefp,*fndefs = ndpart[Afndef].ptr;
  struct param *prmp,*prms = ndpart[Aparam].ptr;

  struct stmt *stmtp,*stmts = ndpart[Astmt].ptr;

  struct rexp *rexpp,*rexps = ndpart[Apexplst].ptr;
  struct prmlst *prmlp,*prmls = ndpart[Aparamlst].ptr;
  struct stmtlst *stmtlp,*stmtls = ndpart[Astmtlst].ptr;

  // struct bexp *bexpp,*bexps = alloc(nbexp + npexp,struct bexp,Mnofil | Mo_ok0,"ast bexp",nextcnt);

  ap->name = name;

  memcpy(ap->ndcnts,ndcnts,sizeof ap->ndcnts);

  ap->gens = gens;
  ap->nhs = nhs;
  ap->fpos = nfps;

  ap->pexps = pexps;
  ap->uexps = uexps;
  ap->bexps = bexps;
  ap->aexps = aexps;

  ap->rexps = rexps;

  ap->ids = ids;
  ap->vars = vars;

  ap->ilits = ilits;
  ap->flits = flits;
  ap->slits = slits;

  ap->asgnsts = asgnsts;

  ap->fndefs = fndefs;
  ap->prms = prms;

  ap->blks = blks;

  ap->ifs = ifs;
  ap->witers = witers;

  ap->stmts = stmts;

  ap->prmls = prmls;
  ap->stmtls = stmtls;

  ap->repool = repool;
  ap->replen = repcnt;

  ap->vars = vars;

  ap->nid = nid;
  ap->uidcnt = uidcnt;
  ap->aidcnt = aidcnt;

  ap->slitpool = slitpool;

  ap->hiblklvl = hiblklvl;

  ub4 napos0,apos = 0;
  ub4 vpos = 0;
  ub4 expcnt,exppos = 0;
  ub8 x8,v8;
  ub4 v4=0;
  bool hasval;

  ub4 xt,xt0;

  ai = 0;

  if (nslit) slithrbuf = alloc(slithilen * 4 + (3 + 4 + 1) * 2,ub1,Mnofil,"ast slit",nextcnt);

  for (apos = 0; apos < argcnt; apos++) {
    x8 = args[apos];
    an  = x8 & hi32; // cid + argc
    pn = x8 >> 32; // pidt + lvl / repos
    lvl = pn >> Argbit;
    pn &= Argmsk;

    ac  = an >> Argbit;
    an &= Argmsk;
    anh = nhs[an];
    at  = anh >> Atybit;
    ani = anh & Atymsk;

    if (at >= Arep) {
      pos = pn;
      if (vpos >= valcnt) ice(0,0,"vpos %u above %u",vpos,valcnt);
      if (apos < 100) info("apos %u vpos %u at %s an %u pn %u %lx",apos,vpos,atynam(at),an,pn,vals[vpos]);
      v8 = vals[vpos++];
      cnt = v8 & hi32;

      if (cnt > repcnt) ice(0,0,"repcnt %u above %u",cnt,repcnt);

      // lvl = v8 >> 32;

    switch (at) {

    case Apexplst: // a + b + ...
      rexpp = rexps + ani;
      if (cnt == 2) { // a + b
        rexpp->cnt = 0;
        lnn = repool[pos];
        lnh = nhs[lnn];
        lt = lnh >> Atybit;
        li = lnh & Atymsk;

        bop = lx2bop(lnh);

        rnn = repool[pos+1];
        rnh = nhs[rnn];
        rt = rnh >> Atybit;
        ri = rnh & Atymsk;

        if (lt == Ailit && rt == Ailit) { // 1 + 1
          ival1 = ilits[li].val;
          ival2 = ilits[ri].val;
          ilits[li].val = beval(ival1,bop,ival2);
          nhs[an] = lnh;
        } else { // a + b
          bexpp = bexps + nbexp; // new bexp node
          bexpp->l = lnn;
          bexpp->op = bop;
          bexpp->r = rnn;
          nhs[an] = Abexp << Atybit | nbexp++;
        }
      } else {
        rexpp->pos = pos;
        rexpp->cnt = cnt;
        exppos += cnt * 2 + 2;
      }
    break;

    case Aparamlst:
      prmlp = prmls + ani;
      prmlp->pos = pos;
      prmlp->cnt = cnt;
    break;

    case Astmtlst:
      stmtlp = stmtls + ani;
      stmtlp->cnt = cnt;
      stmtlp->pos = pos;
      if (cnt > histlstsiz) histlstsiz = cnt;
      info("stmtlst ni %u pos %u cnt %u",ani,pos,cnt);
    break;

    default: break;
    }

    } else { // non rep

      fpx = nfps[an];
      fps = fpx & Lxamsk;

      nh = nhs[pn];
      t = nh >> Atybit;
      ni = nh & Atymsk;
      if (t > Aleaf && t == at) ice(fps,0,"arg %u/%u: identical types vp %u pn %u an %u %s ln %u",apos,argcnt,vpos,pn,an,atynam(t),nlnos[pn]);

      if (apos < 10) sinfo(fps,"ap %3u vp %u pn %3u ni %3u %-6s %-6s ln %u",apos,vpos,pn,ni,atynam(t),atynam(at),nlnos[pn]);

    if (ac & Nodval) { // has val
      hasval = 1;
      ac &= (Nodval-1);
      if (vpos >= valcnt) ice(0,0,"vpos %u above %u",vpos,valcnt);
      v8 = vals[vpos];
      v4 = v8 & hi32;
      xat = fpx >> Lxabit;

      if (apos < 10) sinfo(fps,"ap %3u vp %u ac %u pn %3u ni %3u %-6s %-6s ln %u",apos,vpos,ac,pn,ni,atynam(t),atynam(at),nlnos[pn]);
      vpos++;

      switch (at) {

        case Aid:
          idp = ids + ani;
          sinfo(fps,"idid %u.%u %x.%u %s",an,ani,v4,v4,idnam(v4));
          idp->id = v4;
        break;

        case Avar: break;

        case Ailit:
          if (ani >= nilit) ice(0,0,"ilit ni %u above %u",ni,nilit);
          ilitp = ilits + ani;
          ilitp->val = val;
        break;

        case Aflit:
          flitp = flits + ani;
          fval = (double)(v8 & hi56);
          if (v8 & Bit63) fval = -fval;
          fxs = v8 >> 62;
          fxp = (v8 >> 55) & 0xff;
          if (fxp == 1 && fxs == 0) fval *= 10;
          else if (fxp) {
            if (fxs) fxp = -fxp;
            fval *= exp(10 * log(fxp));
          }
          flitp->val = fval;
        break;

        case Aslit:
          slitp = slits + ani;
          slitp->val = v4;
          str = slitpool + slitstr(v4,&len);
          len = hrslit(str,len,fpx);
          sinfo(fps,"slit %u len %u %.*s",v4,len,len,slithrbuf);
        break;

        case Ailits:
        case Aslits: ice(fps,0,"unexpected type %s",atynam(t));

//        case Aop:    break;

        default:     ice(fps,0,"unexpected type %s",atynam(t));
        }
    } else { // no val
      hasval = 0;
      // sinfo(fps,"ap %3u vp %u pn %3u ni %3u %-6s ln %u",apos,vpos,pn,ni,atynam(t),nlnos[pn]);
    }
    // if (apos < 10) sinfo(fps,"ap %3u pn %3u ni %3u %-6s ln %u",apos,pn,ni,atynam(t),nlnos[pn]);

/* -------
   arg 1
   ------- */

    sinfo(fps,"arg %u %s",ac,atynam(t));

    if (ac == 1) {

    switch (t) {

    case Aid:
      sinfo(fps,"id %u",ni);
    break;

    case Ailits:
      sinfo(fps,"ilits %u",ni);
    break;

    case Aslits:
      str = slits_str(ni,&len,slitpool);
      len = hrslit(str,len,fpx);
      sinfo(fps,"slits %x len %u %.*s",ni,len,len,slithrbuf);
    break;

    case Auexp:
      uexpp = uexps + ni;
      uexpp->op = lx2uop(ani);
      if (uexpp->op > Oucnt) ice(fps,0,"uexp %u.%u invalid op %x.%x",pn,ni,uexpp->op,ani);

#if 0
      if ( (at == Ailits || at == Ailit || at == Aflit) && uop <= Oumin) { nhs[pn] = anh; } // a = -5
      else {
        uexpp = uexps + ni;
        uexpp->e = an;
        uexpp->op = uop;
      }
#endif
    break;

    case Apexp:
      sinfo(fps,"pexp %u %u",ni,an);
      pexpp = pexps + ni;
      pexpp->e = an;
    break;

    case Abexp:
    break;

    case Aaexp:
      aexpp = aexps + ni;
      aexpp->id = an;
    break;

    case Afstr:
    case Afstring:
    break;

    case Aasgnst:
      asgnstp = asgnsts + ni;
      asgnstp->tgt = an;
    break;

    case Ablk:
      if (lvl > prvblklvl) {
        blklvl = 0;
      } else if (lvl < prvblklvl) {
        blklvl++;
        if (blklvl > hiblklvl) hiblklvl = blklvl;
      }
      prvblklvl = lvl;

      blkp = blks + ni;
      blkp->s = an;
      blkp->lvl = blklvl;
      sinfo(fps,"blk %u.%u",pn,ni);
     if (at == Astmt) {
     }
    break;

    case Aif:
      info("ifexp %u e %u",ni,an);
      ifp = ifs + ni;
      ifp->e = an;
    break;

    // iters
    case Awhile:
      witerp = witers + ni;
      witerp->e = an;
    break;

   case Afndef:
     fndefp = fndefs + ni;
     fndefp->id = an;
   break;

   case Aparam:
     prmp = prms + ni;
     prmp->id = an;
     // info("param %u id %u %s",ni,an,atynam(at));
     if (at == Aid) {
       // sinfo(fps,"id %u %u",an,v4);
       if (apos < 10) sinfo(fps,"id %u %s",an,idnam(v4));
     } else ice(fps,0,"expected id, found %u",at);
    break;

    case Astmt:
      stmtp = stmts + ani;
      stmtp->s = an;
    break;

    default: ice(fps,0,"arg %u/%u unhandled type %s",apos,argcnt,atynam(t));
    }

/* -------
   arg 2
 * -------*/

  } else if (ac == 2) {

    switch (t) {

    case Auexp:
      uexpp = uexps + ni;
      uexpp->e = an;
    break;

    case Apexp:
      pexpp = pexps + ni;
      bop = lx2bop(ani);
      if (bop > Obcnt) ice(fps,0,"pexp %u.%u invalid op %x.%x",pn,ni,bop,ani);
      pexpp->op = bop;
    break;

    case Aaexp:
      aexpp = aexps + ni;
      aexpp->e = an;
    break;

    case Aasgnst:
      asgnstp = asgnsts + ni;
      asgnstp->e = an;
    break;

    case Aif:
      info("ifexp %u tru %u",ni,an);
      ifp = ifs + ni;
      ifp->tb = an;
    break;

    // iters
    case Awhile:
      witerp = witers + ni;
      witerp->tb = an;
    break;

   case Afndef:
     fndefp = fndefs + ni;
     fndefp->plst = an;
   break;

   case Aparam:
     prmp = prms + ni;
     prmp->t = an;
    break;

    default: break;
    }
  } else if (ac > 2) {
    ice(fps,0,"unhandled arg %u",ac);
  } // argc 2

  } // rep
 } // each apos

  expcnt = exppos;
  ub4 opos=0,*opspool = alloc(expcnt,ub4,Mnofil | Mo_ok0,"ast exp",nextcnt);
  ub2 ocnt;

  for (ni = 0; ni < nrexp; ni++) {
    rexpp = rexps + ni;
    cnt = rexpp->cnt;
    if (cnt == 0) ice(0,0,"rexpr %u nil cnt",ni);
    info("rexp %u cnt %u",ni,cnt);
    pos = rexpp->pos;
    ocnt = precexp(ap,rexpp,opspool + opos,pos,cnt);
    if (ocnt < 2) ice(0,0,"rexpr %u cnt %u ops %u",ni,cnt,ocnt);
    rexpp->ocnt = ocnt;
    rexpp->opos = opos;
    rexpp->opndx = 0;
    opos += ocnt;
  }
  info("%u` operator pool",opos);

// ---
//  end:
// ---

  if (apos != argcnt) ice(0,0,"arg %u vs %u",apos,argcnt);
  if (vi > valcnt) ice(0,0,"val %u vs %u",vi,valcnt);

  // acnt2 = args - sa->args;
  // if (acnt2 != argcnt) ice(0,0,"argcnt %u vs %u",acnt2,argcnt);
  ap->opspool = opspool;

  ap->root = sa->startnd;
  ap->histlstsiz = histlstsiz;

  showcnt("max blk level",hiblklvl);
  afree(sa->args,"syn args",nextcnt);

  timeit(&T1,"ast build");

  if (globs.rununtil == 5) {
    info("until %u %u",globs.rununtil,gettime_msec()-t1);
    return ap;
  }

  process(ap,emit);

  timeit(&T1,"ast process");

  timeit(&T0,"ast");

  if (nslit) afree(slithrbuf,"ast slitbuf",nextcnt);

//  if (globs.runast) runast(ap,emit);

  return ap;
}
