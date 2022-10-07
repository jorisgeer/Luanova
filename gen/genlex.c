/* genlex.c - generate lexical analyzer code from spec

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

/* A specification lists tokens, keywords and other known identifiers
   The main table describes a state machine with, for each state, a list of :
   <pattern> <next state> <token> <action>

   The pattern can contain character classes as declared in 'sets'
   If next state is omitted, no transition is done
   <token>, if present, causes the given token to be output
   <action> is a reference to freehand code, declared earlier, to execute

   Output is a direct code FSM block.

  */

#include <stdarg.h>
#include <string.h>

#include "../base.h"

#include "../chr.h"
#include "../fmt.h"

#include "../math.h"

#include "../mem.h"
#include "../os.h"

#include "../tim.h"

#define Grp_rep 2

#if defined __clang__
 #pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined __GNUC__
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wpointer-sign"
#endif

static const char version[] = "0.1.0-alpha";

/*
  pat nxstate token action

state: . for no while
pat: ` for no t
nxstate  - for no nxchr
tok   . suffix for hr

*/

#define Lang_blt
#define Lang_dun
#define Lang_stresc

static const bool disabled[128] = {
#ifdef Lang_stresc
  ['a'] = 1,
#else
  ['A'] = 1,
#endif
#ifdef Lang_blt
  ['b'] = 1,
#else
  ['B'] = 1,
#endif
#ifdef Lang_dun
  ['d'] = 1
#else
  ['D'] = 1
#endif
};

static ub1 ctab[256];
static ub1 cctab[256];
static bool cctab_use;

enum Packed8 Ctl { Cc_c,Cc_t,Cc_u,Cc_q,Cc_r,Cc_x,Cc_e,Cc_z,Cc_a};

#define Kwnamlen 16
#define Bltnamlen 24
#define Dunnamlen 16

#define Cclen 4

#define Patcnt 300

static ub4 msgfile = Shsrc_genlex;
#include "../msg.h"

#include "../util.h"

#include "../hash.h"

#ifdef __clang__
 #pragma clang diagnostic ignored "-Wswitch-enum"
#endif

static ub2 msgopts = Msg_shcoord | Msg_fno | Msg_lno | Msg_col | Msg_Lvl;

static const char packed8[] = "Packed8 ";

static const char *specname;
static const char *ltabname;
static const char *lhdrname;
static const char *shdrname;
static bool pass1,pass2;
static ub2 pass;
static char passc;

static cchar *prgnam;

static bool nowrite;
static bool nodiff;
static bool omitcode=0,omittrans=1,omittoken=0;
static char ena_lno = 0;

struct globs globs;

#define Stnam 32

#define Nstate 64
static ub2 nstate;
static cchar *states[Nstate];
static ub1 stlens[Nstate];
static ub2 stlnos[Nstate];
static cchar *stdescs[Nstate];
static ub1 stdesclens[Nstate];

#define Ntok 128
#define Toknam 32
#define Tkgrp 16
static ub2    ntok,nltok;
static cchar *toks[Ntok];
static char   hrtoks[Ntok * 4];
static ub1    toklens[Ntok];
static ub2    toklnos[Ntok];
static ub1    tokgrps[Ntok];
static ub1    tokhrctl[Ntok];
static ub1    tkacts[Ntok];
static ub2    tkrefs[Ntok];
static ub1    tkpass[Ntok];
static ub2    hitknamlen;
static ub1    tkhigrp;
static ub2    tkngrps[Tkgrp];
static ub1    hitkgrp;

#define Nkwd 128
static ub2 nkwd,ntkwd,nkwd_mrg=hi16;
static cchar *kwds[Nkwd];
static ub1 kwlens[Nkwd];
// static ub1 kwgrps[Nkwd];
static ub1 kwlnos[Nkwd];
static ub1 havekwlens[Kwnamlen];

static cchar *tkwds[Nkwd];
static ub1 tkwlens[Nkwd];
static ub1 tkwdmap[Nkwd];

static ub2 hikwlen,mikwlen,lokwlen=255,hitkwlen;

#define Nact 32
#define Actlen 2048
static ub2 nact;
static cchar *acts[Nact];
static ub1 actnlens[Nact];
static ub2 actlns[Nact];
static ub1 actvals[Nact * Actlen];
static ub2 actvlens[Nact];
static ub2 actrefs[Nact];
static ub1 actcnts[Nact];
static ub2 act_init = Nact;
static ub2 act_token = Nact;

#define Nset1 64
#define Nset2 8
#define Nset3 16
#define Nset (Nset1+Nset2+Nset3)
#define Snam 8
#define Setval 512
static ub2 nset,nsets,nsetm,nsetu;
static ub1 setnlens[Nset];
static ub1 setgrps[Nset];
static ub1 setbits[Nset];
static ub1 setval0s[Nset];
static ub1 setcnts[Nset];
static ub1 setmaps[Nset];
static ub2 setlns[Nset];
static bool setcase[Nset];
static ub1 setnams[Nset * Snam];

#define Patlen 16
#define Codlen 2048

struct trans {
  char pat[Patlen];
  ub1 patlen;
  ub1 patno;
  ub1 st0,st;
  ub1 tk,tkhid;
  ub1 act;
  ub1 notpred;
  bool dobt;
  bool actstate;
  bool havecode;
  bool havetok;
  bool hrtok;
  bool dowhile;
  bool setN;
  bool iserr;
  bool tswitch;
  bool eof;
  bool cycle;
  ub1 cclen;
  ub1 syms[Cclen];
  enum Ctl ctls[Cclen];
  ub2 ln;
  ub4 fpos;
  ub1 code[Codlen];
  ub2 codlen;
  cchar *cmt;
  ub2 cmtlen;
};

#define Translen 245
static struct trans transtab[Translen];
static ub2 transtablen;

#define Lxercnt 32
#define Lxernam 16

#define Kwhshiter  1024
#define Dndhshiter 1024

static ub8 chkhsh;

static inline ub4 align4(ub4 x)
{
  ub4 a = x & (~3);

  return a == x ? x : a + 4;
}

static cchar *yesno(bool b) { return b ? "yes" : "no"; }

#undef serror
#define serror(fpos,fmt,...) serrfln(FLN,fpos,fmt,__VA_ARGS__)

static Noret __attribute__ ((format (printf,3,4))) void serrfln(ub4 shfln,ub4 fpos,const char *fmt,...)
{
  va_list ap;

  va_start(ap,fmt);
  vmsgps(shfln,Fatal,fpos,fmt,ap,"lex syntax"," exiting");
  va_end(ap);

  doexit(1);
}

static bool memeq(cchar *p,cchar *q,ub2 n)
{
  ub2 i = 0;
  while (i < n && p[i] == q[i]) i++;
  return i == n;
}

static cchar esctab[] = "abtnvfr";

static cchar *printcchr(ub1 c)
{
  static char buf[8];

  if (c == '\\' || c == '\'') snprintf(buf,8,"'\\%c'",c);
  else if (c >= '\a' && c <= '\r') snprintf(buf,8,"'\\%c'",esctab[c - '\a']);
  else if (Ctab[c]) snprintf(buf,8,"'%c'",c);
  else snprintf(buf,8,"%u",c);
  return buf;
}

static ub1 addstate(ub2 ln,cchar *name,ub1 len)
{
  if (nstate+1 >= Nstate) serror(ln|Lno,"exceeding %u states",nstate);
  states[nstate] = name;
  stlens[nstate] = len;
  stlnos[nstate] = ln;
  return nstate++;
}

static ub1 getstate(cchar *buf,ub2 pos,ub2 len)
{
  ub1 st;

  for (st = 0; st < nstate; st++) {
    if (len == stlens[st] && memeq(states[st],buf+pos,len)) break;
  }
  return st;
}

static ub1 addtok(ub2 ln,cchar *name,ub1 len,ub1 grp)
{
  if (nltok+1 >= Ntok) serror(ln|Lno,"exceeding %u tokens",nltok);
  if (grp < hitkgrp) serror(ln|Lno,"token %.*s grp %u need to be listed ascending, last %u",len,name,grp,hitkgrp);
  hitkgrp = grp;
  toks[nltok] = name;
  toklens[nltok] = len;
  toklnos[nltok] = ln;
  tokgrps[nltok] = grp;
  vrb("add token %u grp %u '%.*s'",nltok,grp,len,name);
  if (nltok) chkhsh = hash64fnv(name,len,chkhsh);
  else chkhsh = hash64(name,len);
  if (len > hitknamlen) hitknamlen = len;
  if (grp > tkhigrp) tkhigrp = grp;
  tkngrps[grp] = nltok;
  return nltok++;
}

static ub1 addact(ub2 ln,cchar *nam,ub1 len)
{
  ub1 act;

  for (act = 0; act < nact; act++) {
    if (len == actnlens[act] && memeq(acts[act],nam,len)) serror(ln|Lno,"action %.*s previously defined at ln %u",len,nam,actlns[act]);
  }

  if (nact+1 >= Nact) serror(ln|Lno,"exceeding %u actions",nact);
  acts[nact] = nam;
  actnlens[nact] = len;
  actlns[nact] = ln;
  if (len == 4 && memeq(nam,"Init",4)) { act_init = nact; actrefs[nact] = ln; }
  else if (len == 5 && memeq(nam,"Token",5)) { act_token = nact; actrefs[nact] = ln; }
  return nact++;
}

static ub1 getact(ub2 ln,cchar *nam,ub2 len)
{
  ub1 act;

  for (act = 0; act < nact; act++) {
    if (len == actnlens[act] && memeq(acts[act],nam,len)) break;
  }
  if (act == nact) serror(ln|Lno,"unknown action %.*s",len,nam);
  return act;
}

static void addactval(ub2 ln,ub2 act,const ub1 *src,ub2 len)
{
  ub2 actlen;
  ub1 *dst;
  ub2 n=len;

  if (act >= nact) serror(ln|Lno,"invalid act %u/%u for '%.*s",act,nact,min(len,16),src);
  if (len == 0) serror(0,"nil len for action %u",act);

  actlen = actvlens[act];
  if (actlen + len + 4 >= Actlen) serror(ln|Lno,"act %.*s exceeds %u code len",actnlens[act],acts[act],Actlen);

  dst = actvals + act * Actlen + actlen;

  while (--n && src[n] == ' ') ;
  if (n == 0 && src[0] == ' ') serror(ln|Lno,"act %u empty line",act);
  len = n+1;

  if (actlen) {
    *dst++ = '\n'; *dst++ = ' '; *dst++ = ' ';
    actlen += 3;
  }
  memcpy(dst,src,len);
  actvlens[act] = actlen + len;
}

static ub2 getset(cchar *p,ub2 len) // return grp-len-set
{
  ub2 t,slen;
  cchar *np;

  for (t = 0; t < nset; t++) {
    slen = setnlens[t];
    np = setnams + t * Snam;
    if (*np != *p || slen > len || (slen > 1 && memcmp(np+1,p+1,slen-1U))) continue;
    return (setgrps[t] << 14 | slen << 8 | t);
  }
  return 0;
}

static void closesets(void)
{
  ub2 i;

  for (i = 0; i < 256; i++) {
    if (ctab[i] == 0xff) ctab[i] = nsets;
  }
}

static ub2 addset(ub2 ln,ub2 grp,const ub1 *nam,ub2 nlen,const ub1 *val,ub2 vlen)
{
  ub2 s,c,d,i,r0,r1,r,u;
  ub2 cnt = 0;
  ub2 lnx = ln|Lno;
  ub2 c2,bit2,x2,s2,x1;
  static ub2 sbit=0,mbit=1;

  if (nset == 0) {
    memset(ctab,0xff,256);
    s = 0;
  } else if (grp == 0 && (nsetm | nsetu)) {
    serror(lnx,"exclusive set %.*s after inclusive",nlen,nam);
  } else {
    for (s = 0; s < nset; s++) {
      if (setnlens[s] == nlen && memeq(setnams + s * Snam,nam,nlen)) serror(lnx,"set %.*s at ln %u redefined",nlen,nam,setlns[s]);
    }
  }

  if (s + 2 >= Nset) serror(lnx,"exceeding %u sets for '%.*s'",s,nlen,nam);

  setnlens[s] = nlen;
  memcpy(setnams + s * Snam,nam,nlen);
  setnams[s * Snam + nlen] = 0;
  setgrps[s] = grp;

  nset++;

  if (grp == 1) {
    if (nsetm + 1 >= min(Nset2,32)) serror(lnx,"exceeding %u multisets at %.*s",Nset2,nlen,nam);
    else if (nsetm + 1 >= 8) swarn(lnx,"exceeding %u multisets at %.*s",Nset2,nlen,nam);
    mbit = 1U << nsetm;
    setbits[s] = mbit;
    nsetm++;
    svrb(lnx,"add set %u/%u +%.*s",s,nsetm,nlen,nam);
  } else if (grp == 0) {
    if (nsets + 1 >= Nset1) serror(lnx,"exceeding %u single sets at %.*s",Nset1,nlen,nam);
    sbit = nsets;
    nsets++;
    setmaps[sbit] = s;
    setbits[s] = sbit;
    svrb(lnx,"add set %u/%u  %.*s",s,nsets,nlen,nam);
  } else if (grp == 2) {
  }

  if (vlen == 2) { // special case for aA
    c = val[0]; d = val[1];
    if (Ctab[c] == Calpha && Ctab[d] == Calpha && ( c == (d | 0x20) || d == (c | 0x20) ) ) {
      setval0s[s] = c | 0x20;
      setcase[s] = 1;
      return s;
    }
  }

  i = 0;

  while (i < vlen) {
    c = val[i];
    if (c == '-' && i && i < vlen - 1) { // a-z
      r0 = val[i-1]; r1 = val[i+1];
      if (r0 >= r1) serror(lnx,"incorrect range '%c' - '%c' in set %.*s",r0,r1,nlen,nam);
      if (grp == 1) {
        for (r = r0 + 1; r <= r1; r++) {
          if (cctab[r] & mbit) {
            serror(lnx,"set %.*s duplicate char '%s' at %c-%c",nlen,nam,chprint(r),r0,r1);
          } else {
            cctab[r] |= mbit; cnt++;
          }
        }
      } else if (grp == 0) {
        for (r = r0 + 1; r <= r1; r++) {
          if (ctab[r] != 0xff) serror(lnx,"set %.*s ch %s already in set #%u",nlen,nam,chprint(r),ctab[r]);
          ctab[r] = sbit;
          cnt++;
        }
      }
      i += 2;
    } else if (c == '+' && grp == 1 && i < vlen - 2) { // add set
      x2 = getset(val + i + 1,2);
      if (x2 == 0) serror(lnx,"set %.*s unknown subset %.2s",nlen,nam,val+i+1);
      s2 = x2 & 0xff;
      bit2 = setbits[s2];
      for (c2 = 0; c2 < 256; c2++) {
        if (ctab[c2] != bit2) continue;
        if (cctab[c2] & mbit) serror(lnx,"set %.*s duplicate char '%s'",nlen,nam,chprint(c2));
        cctab[c2] |= mbit;
        cnt++;
      }
      i += 3;
    } else { // regular
      if (c == '\\' && i + 1 < vlen) { // \xx
        d = val[++i];
        switch (d) {
        case 'n': c = '\n';  break;
        case 'r': c = '\r';  break;
        case 't': c = '\t';  break;
        case 'v': c = '\v';  break;

        case 'Z': // utf8 lead byte
          for (u = 0xc1; u < 0xfe; u++) { if (grp == 1) cctab[u] |= mbit; else if (grp == 0) ctab[u] = sbit; }
          c = 0xc0; cnt += 0xfe - 0xc1;
          break;

        case 'z': // utf8 follow bytes
          for (u = 0x81; u < 0xc0; u++) { if (grp == 1) cctab[u] |= mbit; else if (grp == 0) ctab[u] = sbit; }
          c = 0x80; cnt += 0xc0 - 0x81;
          break;

        case '0':
        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
          c = atox1(d);
          if (i + 1 < vlen) {
            x1 = atox1(val[i+1]);
            if (x1 < 16) { c = (c << 4) | x1; i++; }
          }
          break;
        }
      } // \xx
      if (grp == 1) {
        if (cctab[c] & mbit) {
          serror(0,"set %.*s duplicate char '%s'",nlen,nam,chprint(c));
        } else {
          cctab[c] |= mbit;
        }
      } else if (grp == 0) {
        if (ctab[c] != 0xff) serror(lnx,"set %.*s ch %s already in set #%u",nlen,nam,chprint(c),ctab[c]);
        ctab[c] = sbit;
      } else {
      }
      cnt++;
      if (cnt == 1) setval0s[s] = c;
      i++;
    }
  }

  setcnts[s] = cnt;
  return s;
}

#define err(tp,st,fmt,...) errfln(FLN,tp,st,fmt,__VA_ARGS__)
// #define Bufsiz 256

static Noret void __attribute__ ((format (printf,4,5))) errfln(ub4 shfln,const struct trans *tp,ub1 st,const char *fmt,...)
{
  va_list ap;
  ub4 len = 256;
  char buf[256];
  cchar *stnam = states[st];
  ub2 slen = stlens[st];

  va_start(ap,fmt);
  myvsnprint(buf,0,len,fmt,ap);
  va_end(ap);

  if (tp) serrorfln(shfln,tp->ln | Lno,"pat '%s' state %.*s: %s",tp->pat,slen,stnam,buf);
  else errorfln(shfln,0,"state %.*s: %s",slen,stnam,buf);
  doexit(1);
}

static ub1 getkwd(cchar *name,ub1 len)
{
  ub2 k;

  for (k = 0; k < nkwd; k++) {
    if (len == kwlens[k] && memeq(name,kwds[k],len)) break;
  }
  return k;
}

static void addkwd1(ub2 ln,cchar *name,ub1 len,ub1 grp)
{
  ub2 pos = nkwd;

  if (nkwd+2 >= Nkwd) serror(ln|Lno,"exceeding %u keywords",Nkwd);
  if (len >= Kwnamlen) serror(ln|Lno,"kwd len exceeds %u '%.*s'",Kwnamlen,Kwnamlen,name);

  vrb("add kwd len %u '%.*s'",len,len,name);
  kwds[pos] = name;
  kwlnos[pos] = ln;
  kwlens[pos] = len;
//  kwgrps[pos] = grp;
  if (len > hikwlen) hikwlen = len;
  if (len < lokwlen) lokwlen = len;
  havekwlens[len]++;
  chkhsh = hash64fnv(name,len,chkhsh);
  nkwd = pos + 1;
}

static void addkwd(ub2 ln,cchar *name,ub2 len,ub1 grp)
{
  cchar *n,*eq,*sep;
  ub1 ll;
  ub2 pos = nkwd;

  eq = memchr(name,'=',len);
  if (eq == nil) {
    if (len >= Kwnamlen) serror(ln|Lno,"kwd len exceeds %u '%.*s'",Kwnamlen,Kwnamlen,name);
    tkwdmap[ntkwd] = pos;
    tkwds[ntkwd] = name;
    tkwlens[ntkwd++] = len;
    addkwd1(ln,name,len,grp);
    return;
  } else if (eq == name || eq == name + len - 1) serror(ln|Lno,"invalid kwd '%.*s'",len,name);

  // merged
  if (nkwd_mrg == hi16) nkwd_mrg = nkwd;
  n = name; ll = eq - name;
  addkwd1(ln,eq+1,len - ll - 1,grp);
  while (ll) {
    sep = memchr(n,'|',ll);
    tkwdmap[ntkwd] = pos;
    tkwds[ntkwd] = n;
    if (sep) {
      len = sep - n;
      if (len >= Kwnamlen) serror(ln|Lno,"kwd len exceeds %u '%.*s'",Kwnamlen,Kwnamlen,n);
      tkwlens[ntkwd++] = len;
      hitkwlen = max(hitkwlen,sep-n);
      ll -= sep + 1 - n;
      n = sep + 1;
    } else {
      if (ll >= Kwnamlen) serror(ln|Lno,"kwd len exceeds %u '%.*s'",Kwnamlen,Kwnamlen,n);
      tkwlens[ntkwd++] = ll; hitkwlen = max(hitkwlen,ll);
      break;
    }
  }
}

static struct trans *stdefs[Nstate];
static struct trans *strefs[Nstate];

static ub1 stcover[Nstate];

static ub2 setrefs[Nset];
static ub2 crefs[256];

static ub1 mixrefs[1U << (Cclen * 3)];

static ub2 masktabs[256 * Cclen];

static ub2 addotmask(ub2 tti,ub2 *p)
{
  ub2 c;
  ub2 hiti=tti,ti = hi16;
  bool new = 0;

  for (c = 0; c < 256; c++) {
    ti = p[c];
    if (ti != hi16) hiti = ti;
    else { p[c] = tti; new = 1; }
  }
  return new ? hi16 : hiti;
}

static ub2 addsetmask(ub2 tti,ub2 *p,ub2 set)
{
  ub2 c;
  ub2 hiti=tti,ti;
  bool b,ism = (setgrps[set] == 1);
  bool new = 0;
  ub2 bit = setbits[set];

  for (c = 0; c < 256; c++) {
    if (ism) b = (cctab[c] & bit);
    else b = (ctab[c] == bit);
    if (b == 0) continue;
    ti = p[c];
    if (ti != hi16) hiti = ti;
    else { p[c] = tti; new = 1; }
  }
  return new ? hi16 : hiti;
}

static ub2 addmasks(ub2 tti,ub1 *sp,enum Ctl *cp,ub1 len)
{
  ub2 i,ti = hi16,hiti=hi16;
  ub2 *p;
  ub1 s;
  enum Ctl z;

  for (i = 0; i < len; i++) {
    p = masktabs + i * 256;
    s = sp[i];
    z = cp[i];
    switch (z) {
      case Cc_c: ti = p[s]; if (ti != hi16) hiti = ti; else p[s] = tti; break;
      case Cc_t:
      case Cc_u: ti = addsetmask(tti,p,s); if (ti != hi16) hiti = ti; break;
      case Cc_q: return hi16;
      case Cc_r: return hi16;
      case Cc_x: return hi16;
      case Cc_e: return hi16;
      case Cc_z: ti = p[0]; if (ti != hi16) hiti = ti; else p[0] = tti; break;
      case Cc_a: ti = p[s]; if (ti == hi16) { p[s] = tti; break; }
                 else { ti = upcase(p[s]); hiti = ti; }
                 break;
    }
    if (ti == hi16) break;
  }
  if (ti == hi16) return hi16;
  else return hiti == tti ? hi16 : hiti;
}

static int mktables(void)
{
  struct trans *tp,*tp0,*tp2,*tpmx=nil;
  ub1 st,st0;
  ub1 tk;
  const char *p;
  char c,d;
  ub1 sym,*sp;
  enum Ctl ctl,*cp;
  ub1 t;
  ub2 mix;
  ub2 len;
  ub4 pi;
  ub2 ln;
  ub2 ttndx0=0,ttndx = 0,tti;
  ub2 cc,cclen,ccmax,ccmxv=0;
  ub2 i;
  ub2 x2,setgrp;
  ub2 plen;
  ub2 set;
  ub2 act;
  ub2 patcnt=0;
  bool noset=0;
  bool hrtok,defhr;
  char *hp;

  ccmax = 0;

  st0 = transtab->st0;
  ttndx0 = 0;
  tp0 = transtab;
  while (ttndx < transtablen) { // each entry
    tp = transtab + ttndx;

    if (omitcode == 0) {
      if (tp->codlen || ( (act = tp->act) < nact && actvlens[act]) ) tp->havecode = 1;
    }
    if (omittoken == 0) {
      if (tp->tk < nltok) tp->havetok = 1;
    }

    if (tp->st0 != st0) {
      st0 = tp->st0; patcnt = 0;
      ttndx0 = ttndx;
      tp0 = transtab + ttndx;
    } else patcnt++;

    if (patcnt >= Patcnt) err(tp,st0,"exceeding %u patterns",Patcnt);

    st = tp->st;
    p = tp->pat;
    plen = tp->patlen;
    ln = tp->ln;
    tk = min(tp->tk,tp->tkhid);
    if (tk < nltok) hrtok = tp->hrtok;
    else hrtok = 0;
    if (*p == 0 || plen == 0) err(tp,st0,"empty pat at %u",ttndx);

    cclen = 0;
    sp = tp->syms;
    cp = tp->ctls;
    mix = 0;
    noset = tp->dowhile;

    defhr = (st0 == 0 && tk < nltok && plen < 4);

    pi = 0; c = p[0];

    if (plen > 2 && c == '~') {
      if (p[1] != '~') {
        tp->notpred = p[1];
        pi = 2;
      } else pi = 1;
    } else if (plen > 1 && c == '.') {
      if (p[1] != '.') noset = 1;
      pi = 1;
    }

    while (pi < plen) {
      c = p[pi]; d = p[pi+1];
      len = plen - pi;
      sym = c;
      ctl = Cc_c;
      if (c == 'Q') { // compare input with current quote
        ctl = Cc_q;
        pi++;
      } else if (len >= 2 && c == 'o' && d == 't') { // other
        ctl = Cc_x;
        pi = plen;
      } else if (len >= 2 && c == 'E' && d == 'Q') { // equal
        if (pi == 0) err(tp,st0,"EQ at start, pat %.*s",plen,p);
        ctl = Cc_e;
        pi+=2;
      } else if (len >= 3 && c == 'R') { // compare register against given val
        if (d == 'R') { pi += 2; sym = 'R'; }
        else if (d >= '0' && d <= '9' && p[pi+2] >= '0' && p[pi+2] <= '9') {
          ctl = Cc_r; sym = ((d - '0') << 4) | (p[pi+2] - '0'); pi += 3;
        } else { pi++; sym = c; }
      } else if (c == 0) {
        err(tp,st0,"nil pattern char at pos %u",pi);
      } else {
        x2 = getset(p+pi,plen-pi);
        if (x2 == 0) {
          c = p[pi++];
          for (set = 0; set < nset; set++) {
            if (setgrps[set] != 2 && setcnts[set] == 1 && setval0s[set] == c) break;
          }
          if (set < nset && noset == 0 && cclen == 0) { // convert to 1-char set
            setrefs[set] = ln;
            sym = set;
            ctl = setgrps[set] ? Cc_u : Cc_t;
          } else {
            sym = c;
            ctl = Cc_c;
            crefs[c]++;
          }

        } else { // is set
          set = x2 & 0xff;
          setgrp = x2 >> 14;
          setrefs[set] = ln;
          if (setcase[set]) {
            ctl = Cc_a;
            sym = setval0s[set];
          } else if (noset) {
            if (setcnts[set] != 1) err(tp,st0,"char override on len %u set %u",setcnts[set],set);
            sym = setval0s[set];
            if (st0 == 0) vrb("ln %u.%u pat '%s' noset for root",ln,cclen,chprint(sym));
            ctl = Cc_c;
          } else if (setgrp == 1) { // mul
            sym = set;
            ctl = Cc_u;
          } else if (setgrp == 2) { // usr
            if (setcnts[set] != 1) err(tp,st0,"len %u user set %u",setcnts[set],set);
            sym = setval0s[set];
            ctl = Cc_c;
          } else { // normal t-set
            sym = set;
            ctl = Cc_t;
          }
          len = (x2 >> 8) & 0xf;
          pi += len;
        }
      } // is set
      noset=0;
      mix = mix << 3 | ctl;
      if (ctl == Cc_u) cctab_use = 1;
      sp[cclen]   = sym;
      cp[cclen++] = ctl;
      if (cclen > ccmax) { ccmax = cclen; ccmxv = tp->ln; tpmx = tp; }
      if (cclen > Cclen) {
        for (i = 0; i < cclen; i++) {
          info("  type %u val %x",cp[i],sp[i]);
        }
        err(tp,st0,"unsupported cc len %u",cclen);
      }
    } // each pat item

    // check overlap
    if (ttndx > ttndx0) {
      memset(masktabs,0xff,256 * Cclen * sizeof(ub2));
      for (tti = ttndx0; tti < ttndx; tti++) {
        tp2 = transtab + tti;
        if (tp2->cclen > cclen) continue;
        addmasks(tti,tp2->syms,tp2->ctls,tp2->cclen);
      }
      tti = addmasks(ttndx,sp,cp,cclen);
      if (tti != hi16) {
        tp2 = transtab + tti;
        err(tp,st0,"pattern '%s' shadowed by ln %u '%s'",tp->pat,tp2->ln,tp2->pat);
      }
    }

    tp->cclen = cclen;
    if (st == st0) tp0->cycle = 1;
    mixrefs[mix]++;

    ttndx++;
  } // each pat

  if (msgerrcnt()) return 1;

  for (t = 0; t < nset; t++) {
    if (setrefs[t]) continue;
    sgenmsg(Lno,setgrps[t] ? Info : Error,"ctype %.*s unreferenced",setnlens[t],setnams + t * Snam);
  }

  for (x2 = 0; x2 < 256; x2++) {
    if (crefs[x2]) vrb("%u %s",crefs[x2],chprint(x2));
  }

  info("max cc %u at ln %u '%s'",ccmax,ccmxv,tpmx->pat);

  for (i = 0; i < (1U << (Cclen * 3)); i++) {
    x2 = mixrefs[i];
    if (x2) vrb("cc spec 0x%x = %u",i,x2);
  }

  return 0;
}

enum Specstate { Sout,Scmt,Spcmt,
  Svarnam,Svarnam1,Svarval,
  Skwd0,Skwd,Skwd1,Skwd2,
  Stoknam0,Stoknam,Stoknam1,Stokgrp,Shrtok0,Shrtok,
  Sstate0,Sstate1,Sstate2,Sstate3,
  Spat0,Spat1,Spat2,Snxstate0,Snxstate1,
  Sact0,Sact1,Sact2,Sact20,Sact21,Sact3,
  Sset0,Sset1,Sset2,Sset3,Sset4,
  Stoken0,Stoken1,
  Spatact0,Spatact1,Scode,Sccmt,Sstate4,
  Seof,Scount };

#define Specvlen 128
static char specversion[Specvlen];
static char specdate[Specvlen];
static char specreq[Specvlen];
static char specauthor[Specvlen];
static char speclang[Specvlen];
static char speceof[Specvlen];

static char specxdate[64];

enum Specvar { Sv_version,Sv_author,Sv_date,Sv_requires,Sv_lang,
  Sv_tkeof,
  Sv_kwd,Sv_set,Sv_bltin,Sv_dunder,
  Sv_token,
  Sv_action,
  Sv_table,Sv_count };

static const char *svnames[Sv_count] = {
 [Sv_version]  = "version",
 [Sv_author]   = "author",
 [Sv_date]     = "date",
 [Sv_requires] = "requires",
 [Sv_lang]     = "language",
 [Sv_tkeof]    = "eof",
 [Sv_table]    = "table",
 [Sv_kwd]      = "keyword",
 [Sv_set]      = "set",
 [Sv_bltin]    = "builtin",
 [Sv_dunder]   = "dunder",
 [Sv_action]   = "action",
 [Sv_token]    = "token"
};

static ub2 svfpos[Sv_count];
static ub2 specvlens[Sv_count];

static enum Specvar getvar(cchar *p,ub2 nam0,ub2 nam1)
{
  ub2 namlen,nlen;
  enum Specvar sv,sv2;
  enum Msglvl lvl;

  if (nam1 <= nam0) serror(nam0,"invalid length %u-%u for var name",nam0,nam1);
  namlen = nam1 - nam0;
  if (namlen > 64) serror(nam0,"invalid length %u for var name",namlen);

  for (sv = 0; sv < Sv_count; sv++) {
    nlen = strlen(svnames[sv]);
    if (namlen == nlen && memeq(svnames[sv],p+nam0,namlen)) break;
  }

  if (sv == Sv_count) serror(nam0,"unknown var '%.*s'",namlen,p+nam0);

  if (sv == Sv_table) {
    for (sv2 = 0; sv2 < Sv_count; sv2++) {
      if (sv2 != Sv_table && svfpos[sv2] == 0) {
        switch (sv2) {
          case Sv_date:   lvl = Vrb;  break;
          case Sv_action:
          case Sv_kwd:
          case Sv_token:  lvl = Info; break;
          case Sv_bltin:
          case Sv_dunder:
          case Sv_tkeof:  lvl = Vrb; break;
          default:        lvl = Warn;
        }
        genmsg(lvl,"var %s undefined",svnames[sv2]);
      }
    }
  }
  if (msgerrcnt()) doexit(1);

  if (svfpos[sv]) {
    sinfo0(svfpos[sv],"previously defined");
    serror(nam0,"var %s redefined",svnames[sv]);
  }
  svfpos[sv] = nam0;
  vrb("var %.*s",namlen,p+nam0);
  return sv;
}

static void getval(enum Specvar sv,cchar *p,ub4 nam0,ub4 namlen,ub2 val0,ub2 val1)
{
  ub2 vallen;
  char *dst;

  if (val1 <= val0) serror(val0,"invalid length %u-%u for var %.*s",val0,val1,namlen,p+nam0);
  vallen = val1 - val0;
  if (vallen >= Specvlen) serror(val0,"length %u for var %.*s above %u",vallen,namlen,p+nam0,Specvlen);

  switch (sv) {
  case Sv_version:  dst = specversion; break;
  case Sv_author:   dst = specauthor;  break;
  case Sv_date:     dst = specdate;    break;
  case Sv_lang:     dst = speclang;    break;
  case Sv_requires: dst = specreq;     break;
  case Sv_tkeof:    dst = speceof;     break;
  default: return;
  }
  memcpy(dst,p+val0,vallen);
  specvlens[sv] = vallen;
}

#define Nblt 256
static ub2 nblt;
static cchar *blts[Nblt];
static ub1 bltlens[Nblt];
static ub2 bltlnos[Nblt];
static ub1 havebltlens[Bltnamlen];
static ub2 lobltlen=255,hibltlen,mibltlen;

static void addblt(ub2 ln,cchar *name,ub1 len)
{
  if (nblt+1 >= Nblt) serror(ln|Lno,"exceeding %u builtins",nblt);
  blts[nblt] = name;
  bltlens[nblt] = len;
  bltlnos[nblt] = ln;
  havebltlens[len]++;
  if (len > hibltlen) hibltlen = len;
  if (len < lobltlen) lobltlen = len;
  nblt++;
  chkhsh = hash64fnv(name,len,chkhsh);
}

static ub2 getblt(cchar *name,ub1 len)
{
  ub2 blt;

  for (blt = 0; blt < nblt; blt++) {
    if (len == bltlens[blt] && memeq(name,blts[blt],len)) return blt;
  }
  return nblt;
}

#define Ndun 256
static ub2 ndun;
static cchar *duns[Ndun];
static ub1 dunlens[Ndun];
static ub2 dunlnos[Ndun];
static ub1 havedunlens[Dunnamlen];
static ub2 lodunlen=255,hidunlen;

static void adddun(ub2 ln,cchar *name,ub1 len)
{
  if (ndun+1 >= Ndun) serror(ln|Lno,"exceeding %u dunders",ndun);
  duns[ndun] = name;
  dunlens[ndun] = len;
  dunlnos[ndun] = ln;
  havedunlens[len]++;
  if (len > hidunlen) hidunlen = len;
  if (len < lodunlen) lodunlen = len;
  ndun++;
  chkhsh = hash64fnv(name,len,chkhsh);
}

static ub2 getdun(cchar *name,ub1 len)
{
  ub2 dun;

  for (dun = 0; dun < ndun; dun++) {
    if (len == dunlens[dun] && memeq(name,duns[dun],len)) return dun;
  }
  return ndun;
}

static ub1 gettoken(ub2 ln,cchar *buf,ub4 pos,ub2 len)
{
  ub1 tk;

  for (tk = 0; tk < nltok; tk++) {
    if (len == toklens[tk] && memeq(toks[tk],buf+pos,len)) return tk;
  }
  serror(ln|Lno,"unknown token '%.*s'",len,buf+pos);
}

#define Hshlen 0x1000
static ub1 kwhsh[Hshlen];
static ub1 blthsh[Hshlen];
static ub1 dun0hsh[Hshlen];
static ub1 dun1hsh[Hshlen];
static ub2 kwhshlen,blthshlen,dun0hshlen,dun1hshlen;
static ub1 kwhshlut[256];
static ub1 blthshlut[256];
static ub4 hshseed,d0hshseed,d1hshseed;
static ub2 kwdhshshift,blthshshift,dun0hshshift,dun1hshshift;

static void setlut(ub1 *lut,ub4 hc,ub1 bit)
{
  lut[hc & 0xff] |= (1U << bit);
  lut[(hc >>  8) & 0xff] |= (2U << bit);
  lut[(hc >> 16) & 0xff] |= (4U << bit);
  lut[hc >> 24] |= (8U << bit);
}

// create hash and check if perfect
static ub2 mkkwhsh2(ub1 *hsh,cchar *strs[],ub1 *slens,ub2 cnt,ub2 len,ub1 pwr2,ub4 seed)
{
  const char *p;
  ub4 hc,len1 = len-1;
  ub2 h,slen,sno;
  ub1 bit;

 for (bit = 0; bit < 32 - pwr2; bit++) {

  memset(hsh,cnt,len);
  memset(kwhshlut,0,256);

  for (sno = 0; sno < cnt; sno++) {
    slen = slens[sno];
    if (slen < 3) continue;
    p = strs[sno];
    if ((size_t)p & 3) hc = hashstr(p,slen,seed);
    else hc = hashalstr(p,slen,seed);

    // info("len %-4u h %x %.*s",len,hc,slen,p);
    setlut(kwhshlut,hc,slen >= mikwlen ? 4 : 0);

    hc = (hc >> pwr2) ^ hc;
    if (bit) hc >>= bit;
    h = hc & len1;

    if (hsh[h] < cnt) break; // collided
    hsh[h] = sno;
  }
  if (sno == cnt) return bit;
 }
 return 32;
}

static ub2 mkblthsh2(ub1 *hsh,cchar *strs[],ub1 *slens,ub2 cnt,ub2 len,ub1 pwr2,ub4 seed)
{
  const char *p;
  ub4 hc,len1 = len-1;
  ub2 h,slen,sno;
  ub1 bit;

 for (bit = 0; bit < 32 - pwr2; bit++) {

  memset(hsh,cnt,len);
  memset(blthshlut,0,256);

  for (sno = 0; sno < cnt; sno++) {
    slen = slens[sno];
    if (slen < 3) continue;
    p = strs[sno];
    if ((size_t)p & 3) hc = hashstr(p,slen,seed);
    else hc = hashalstr(p,slen,seed);

    setlut(blthshlut,hc,slen >= mibltlen ? 4 : 0);

    hc = (hc >> pwr2) ^ hc;
    if (bit) hc >>= bit;
    h = hc & len1;

    if (hsh[h] < cnt) break; // collided
    hsh[h] = sno;
  }
  if (sno == cnt) return bit;
 }
 return 32;
}

static ub2 mkdunhsh2(ub1 *hsh,cchar *strs[],ub1 *slens,ub2 cnt,ub2 len,ub1 pwr2,ub4 seed,ub1 part)
{
  const char *p;
  ub4 hc,len1 = len-1;
  ub2 h,slen,sno;
  ub2 bit;

 for (bit = 0; bit < 32 - pwr2; bit++) {

  memset(hsh,cnt,len);

  for (sno = 0; sno < cnt; sno++) {
    slen = slens[sno];
    if (slen < 3) continue;
    p = strs[sno];
    if ((*p & 1) != part) continue;

    if ((size_t)p & 3) hc = hashstr(p,slen,seed);
    else hc = hashalstr(p,slen,seed);

    hc = (hc >> pwr2) ^ hc;
    if (bit) hc >>= bit;
    h = hc & len1;

    if (hsh[h] < cnt) break;
    hsh[h] = sno;
  }
  if (sno == cnt) return bit;
 }
 return 32;
}

static ub2 mkkwhsh(ub2 len,ub4 seed)
{
  ub2 bit = 0,len2 = 1,sbit;

  // check next pwr2
  while (len2 < len) { len2 <<= 1; bit++; }

  while (len2 < Hshlen) {
    sbit = mkkwhsh2(kwhsh,tkwds,tkwlens,ntkwd,len2,bit,seed);
    if (sbit < 32) {
      kwdhshshift = sbit;
      return len2;
    }
    len2 <<= 1; bit++;
  }
  return Hshlen;
}

static ub2 mkbltsh(ub2 len,ub4 seed)
{
  ub2 bit = 0,len2 = 1,sbit;

  while (len2 < len) { len2 <<= 1; bit++; }

  while (len2 < Hshlen) {
    sbit = mkblthsh2(blthsh,blts,bltlens,nblt,len2,bit,seed);
    if (sbit < 32) {
      blthshshift = sbit;
      return len2;
    }
    len2 <<= 1; bit++;
  }
  return Hshlen;
}

static ub2 mkdunhsh(ub2 len,ub4 seed,ub1 part)
{
  ub2 bit = 0,len2 = 1,sbit;
  ub1 *hsh = part ? dun1hsh : dun0hsh;

  while (len2 < len) { len2 <<= 1; bit++; }

  while (len2 < Hshlen) {
    sbit = mkdunhsh2(hsh,duns,dunlens,ndun,len2,bit,seed,part);
    if (sbit < 32) {
      if (part) dun1hshshift = sbit;
      else dun0hshshift = sbit;
      return len2;
    }
    len2 <<= 1; bit++;
  }
  return Hshlen;
}

static int mkhshes(void)
{
  ub2 n,klen=0,blen=0,dlen,loklen=Hshlen,loblen=Hshlen,lodlen=Hshlen;
  ub2 sum = 0;
  ub4 it,loit=0,loseed=0,seed = 0;

  if (ntkwd >= 255) { error("%u kwd exceeds 255",ntkwd); return 1; }

  for (n = 3; n < Bltnamlen; n++) {
    sum += havebltlens[n];
    if (sum * 2 >= nblt) break;
  }
  mibltlen = n;

  sum = 0;
  for (n = 3; n < Kwnamlen; n++) {
    sum += havekwlens[n];
    if (sum * 2 >= ntkwd) break;
  }
  mikwlen = n;

  for (it = 0; it < Kwhshiter; it++) {
    if (ntkwd) klen = mkkwhsh(ntkwd,seed);
    if (nblt) blen = mkbltsh(nblt,seed);
    if (klen + blen < loklen + loblen) { loklen = klen; loblen = blen; loseed = seed; loit=it; }
    seed = rnd(hi32);
  }

  if (ntkwd) {
    if (loklen >= Hshlen) serror(Lno,"no hash len under %u found for kwd",Hshlen);
    klen = mkkwhsh(loklen,loseed);
    kwhshlen = klen;
    info("kwd    hash size %4u for %2u seed %x at iter %u shift %u",klen,ntkwd,loseed,loit,kwdhshshift);
  }
  if (nblt) {
    if (loblen >= Hshlen) serror(Lno,"no hash len under %u found for bltin",Hshlen);
    blen = mkbltsh(loblen,loseed);
    blthshlen = blen;
    info("blt    hash size %4u for %2u shift %u",blen,nblt - havebltlens[2],blthshshift);
  }
  hshseed = loseed;

  if (ndun == 0) return 0;

  seed = 0;
  for (it = 0; it < Dndhshiter; it++) {
    dlen = mkdunhsh(ndun,seed,0);
    if (dlen < lodlen) { lodlen = dlen; loseed = seed; loit=it; }
    seed = rnd(hi32);
  }
  if (lodlen >= Hshlen) serror(Lno,"no hash len under %u found for dunder",Hshlen);
  dlen = mkdunhsh(lodlen,loseed,0);
  if (dlen != lodlen) ice(0,0,"dunder hash %u vs %u",dlen,lodlen);
  dun0hshlen = dlen;
  info("dunder hash 0 size %4u for %2u seed %x at iter %u shift %u",dlen,ndun - havedunlens[2],loseed,loit,dun0hshshift);
  d0hshseed = loseed;

  seed = 0; lodlen = Hshlen;
  for (it = 0; it < Dndhshiter; it++) {
    dlen = mkdunhsh(ndun,seed,1);
    if (dlen < lodlen) { lodlen = dlen; loseed = seed; loit=it; }
    seed = rnd(hi32);
  }
  if (lodlen >= Hshlen) serror(Lno,"no hash len under %u found for dunder",Hshlen);
  dlen = mkdunhsh(lodlen,loseed,1);
  if (dlen != lodlen) ice(0,0,"dunder hash %u vs %u",dlen,lodlen);
  dun1hshlen = dlen;
  info("dunder hash 1 size %4u for %2u seed %x at iter %u shift %u",dlen,ndun - havedunlens[2],loseed,loit,dun1hshshift);
  d1hshseed = loseed;


  return 0;
}

static void addstates(ub2 ln,cchar *src,ub2 n)
{
  ub1 c,d;
  ub2 nam0,len;
  ub2 st;
  ub2 lno = ln;
  enum Ctype t,t2;

  while (src[n] && src[n] != '\n') n++;

  do {
    nam0 = n;
    c=src[n];
    if (c == 0) break;
    d=src[n+1];
    t = Ctab[c];
    t2 = Ctab[d];
    switch (t) {
    case Cdot:   nam0 = n+1; break;
    case Calpha: break;
    case Cnl:    lno++; break;
    case Chsh:   if (src[n+1] == '#') goto end;
    default:     break;
    }
    n++;

   if (t == Calpha || (t == Cdot && t2 == Calpha) ) {

    do {
      t = Ctab[src[++n]];
    } while (t == Calpha || t == Cnum);

    len = n - nam0;

    svrb(n,"add state %u '%.*s'",nstate,len,src+nam0);

    if (len + 1 >= Stnam) serror(lno|Lno,"state name '%.*s' exceeds len %u",Stnam,src+nam0,Stnam);
    st = getstate(src,nam0,len);
    if (st < nstate) serror(lno|Lno,"state '%.*s' previously defined at line %u",len,src+nam0,stlnos[st]);
    addstate(lno,src+nam0,len);

   } else if (t == Cnl) continue;

    while (src[n] && src[n] != '\n') n++;
  } while (1);

end:

  if (nstate == 0) serrorfln(FLN,Lno,"no states");
  if (nstate == 1) swarnfln(FLN,Lno,"only one state");

  showcnt("2state",nstate);
}

static ub1 statereach[Nstate * Nstate];

static int rdspec(cchar *fname)
{
  enum Specstate xst2 = Sout,xst = Sout;
  ub1 st00,st=0,st0 = nstate;
  enum Ctype t;
  ub1 tk=0;
  ub1 grp=0,setgrp=0;
  ub2 kw;
  ub2 blt;
  ub2 dun;
  ub2 act=0;
  ub2 codlen;

  ub1 *streach = statereach;
  char stnam[Stnam];

  struct myfile specfile;

  ub2 lno = 1,otherln = 0;
  ub4 lncnt;
  ub4 *lntab;

  struct trans *tp,*tp1,*tp2;

  enum Specvar sv = Sv_count;
  ub2 c,c2;
  bool actena = 1,actself = 0;

  if (readfile_pad(&specfile,fname,1,hi24,4,0)) return 1;

  const ub1 *buf = specfile.buf;
  ub4 len = specfile.len;
  if (len == 0) { error("%s is empty",fname); return 1; }

  ub4 specdmin = (ub4)(specfile.mtime / 60);
  ub4 specdtim = nixday2cal(specdmin / (24 * 60));

  lntab = mklntab(buf,len,&lncnt);

  ub2 patno=0;
  ub2 set;
  ub2 n = 0;
  ub2 cnt;
  ub2 varnam0=0,varnam1=0,varval0=0,varval1=0;
  ub2 kwdnam0=0;
  ub2 slen=0,i;
  ub2 idnam0=0,idnam1=0,idlen;
  ub2 toknam0=0,toknlen=0;
  ub2 setnam0=0,setnam1=0,setnlen=0,setval0=0,setval1=0;
  ub2 actnam0=0,actnam1=0,actval0=0,actval1=0;
  const ub1 *code;

  ub2 hrdups[256];

  bool multiset=0;
  bool tokeep=0;
  bool skipact=0;
  bool enable=1;

  memset(hrtoks,' ',Ntok * 4);
  memset(hrdups,0,512);

  tp = transtab;

  n = 0;

  do {
    c = buf[n];
    if (c == 0) break;
    c2 = buf[n+1];

    t = Ctab[c];

    if (t == Cnl) lno++;

  switch (xst) {
  case Scount: break;

  case Scmt:
    if (c == '\n') {
      xst = xst2;
    } else if (c == 0) { sinfofln(FLN,lno,"eof in comment"); goto eof; }
    break;

  case Sout:
    switch (t) {
    case Cws:  break;
    case Cnl:  break;
    case Chsh: if (c2 == '#') { sinfofln(FLN,n,"eof at blockcomment"); goto eof; }
               else { xst = Scmt; xst2 = Sout; break; }
    case Cbtk: enable = !disabled[c2]; n++; break;
    case Cpls:
    case Cmin:
    case Calpha: varnam0 = n; varnam1=0; xst = Svarnam; break;
    case Cnum:   serror(n,"expected keyword, found %c",c);
    default:     serror(n,"expected keyword, found %c as %u",c,t);
    }
    break;

  case Svarnam:
    switch (t) {
    case Calpha: case Cnum: case Cmin: break;
    case Cws:
    case Chsh:
    case Cnl: varnam1 = n; break;
    default: serror(n,"expected <var> <value>, found %c",c);
    }
    if (varnam1 == 0) break;
    // info("'%.*s'",varnam1-varnam0,buf+varnam0);
    sv = getvar(buf,varnam0,varnam1);
    actena = 1;
    switch (sv) {
    case Sv_count:  xst = Scmt;     break; // unknown
    case Sv_kwd:
    case Sv_bltin:
    case Sv_dunder: xst = Skwd0;    break;
    case Sv_token:  xst = Stoknam0; break;
    case Sv_action: xst = Sact0;    break;
    case Sv_set:    xst = Sset0;    break;
    case Sv_table:  xst = Sstate0;
                    if (nstate == 0) addstates(lno,buf,n);
                    break;
    default: xst = Svarnam1;
    }

  break;

  case Svarnam1:
    if (t == Cnl) serror(n,"missing value for %.*s",varnam1-varnam0,buf+varnam0);
    else if (t != Cws) { varval0 = n; varval1 = 0; xst = Svarval; }
    else if (c == 0) serror(n,"ln %u unexpected eof",lno);
  break;

  case Svarval:
    switch (t) {
//  case Chsh: varval1 = n; xst = Scmt; xst2 = Sout; break;
    case Cnl:  varval1 = n; xst = Sout; break;
    default:   if (Ctab[c] == 0 || c == '\r') serror(n,"nonprintable char '%s'",chprint(c));
    }
    if (varval1 == 0) break;

    while (varval1 > varval0 + 1U && buf[varval1] == ' ') varval1--;
    getval(sv,buf,varnam0,varnam1-varnam0,varval0,varval1);
  break;

  // keywords / bltins
  case Skwd0:
    switch (t) {
    case Cnl:    break;
    case Cbtk:   enable = !disabled[c2]; n++; break;
    case Calpha: if (buf[n-1] == '\n') enable = 1;
                 varnam0 = n; varnam1=0; xst = Svarnam;
                 break;
    case Cws:    xst = Skwd1; break;
    case Cdot:
    case Chsh:   xst = Scmt; xst2 = Skwd0; break;
    default:     serror(n,"expected keyword, found '%c'",c);
    }
  break;

  case Skwd1:
    switch (t) {
    case Cws: break;
    case Calpha: kwdnam0 = n; xst = Skwd; break;
    case Chsh:   xst = Scmt; xst2 = Skwd0; break;
    case Cnl:    xst = Skwd0; break;
    default:     serror(n,"expected keyword, found %c",c);
    }
  break;

  case Skwd:
    switch (t) {
    case Calpha: case Cnum:
    case Cor: case Ceq:
      slen = n - kwdnam0;
      if (slen >= max(Kwnamlen,Bltnamlen)) serror(kwdnam0,"%s '%.*s' len exceeds %u",svnames[sv],slen,buf+kwdnam0,slen);
      break;

    case Cws: case Cnl: case Chsh:
      slen = n - kwdnam0;

      if (sv == Sv_dunder) {
        dun = getdun(buf+kwdnam0,slen);
        if (dun < ndun) serror(kwdnam0,"%.*s already defined as dunder at line %u",slen,buf+kwdnam0,dunlnos[dun]);
        if (enable) adddun(lno,buf+kwdnam0,slen);
      } else {
        kw = getkwd(buf+kwdnam0,slen);
        if (kw < nkwd) serror(kwdnam0,"%.*s already defined as kwd at line %u",slen,buf+kwdnam0,kwlnos[kw]);
        blt = getblt(buf+kwdnam0,slen);
        if (blt < nblt) serror(kwdnam0,"%.*s already defined as bltin at line %u",slen,buf+kwdnam0,bltlnos[blt]);

        if (sv == Sv_kwd && t != Cws) addkwd(lno,buf+kwdnam0,slen,0);
        else if (enable && sv == Sv_bltin) addblt(lno,buf+kwdnam0,slen);
      }

      if (t == Cnl) xst = Skwd0;
      else if (t == Cws && sv == Sv_kwd) { xst = Skwd2; grp = 0; }
      else { xst = Scmt; xst2 = Skwd0; }
      break;
    default:   serror(n,"expected %s, found %c",svnames[sv],c);
    }
  break;

  case Skwd2:
  switch (t) {
  case Cws: break;
  case Cnum: grp = c - '0'; break;
  case Cnl:  addkwd(lno,buf+kwdnam0,slen,grp); xst = Skwd0; break;
  case Chsh: addkwd(lno,buf+kwdnam0,slen,grp); xst = Scmt; xst2 = Skwd0; break;
  default:   serror(n,"expectedgrp digit, found %c",c);
  }
  break;

  case Sset0:
    setgrp=0;
    switch (t) {
    case Cnl:    break;
    case Calpha: varnam0 = n; varnam1=0; xst = Svarnam; break;
    case Cws:    xst = Sset1; break;
    case Cdot:
    case Chsh:   xst = Scmt; xst2 = Sset0; break;
    default:     serror(n,"expected set, found %c",c);
    }
  break;

  case Sset1:
    switch (t) {
    case Cws:    break;
    case Cpls:   setgrp = 1; break;
    case Cmin:   setgrp = 2; break;
    case Calpha:
    case Cnum:   setnam0 = n; xst = Sset2; break;
    case Chsh:   xst = Scmt; xst2 = Sset0; break;
    case Cnl:    xst = Sset0; break;
    default:     serror(n,"expected set, found %c",c);
    }
  break;

  case Sset2:
    switch (t) {
    case Calpha: case Cnum: break;
    case Chsh:
    case Cnl:    serror(n,"empty set %.*s",setnlen,buf+setnam0);
    case Cws:    setnam1 = n; setnlen = setnam1-setnam0; xst = Sset3; break;
    default:     serror(n,"expected set, found %c",c);
    }
  break;

  case Sset3:
    switch(t) {
    case Cws:    break;
    case Cnl:    serror(n,"empty set %.*s",setnlen,buf+setnam0);
    default:     if (Ctab[c] == 0 || c == '\r') serror(n,"nonprintable char '%s'",chprint(c));
                 setval0 = n; setval1 = 0; xst = Sset4;
    }
  break;

  case Sset4:
    if (n - setval0 >= Setval) serror(n,"set %.*s length exceeds %u",setnlen,buf+setnam0,Setval);
    switch (t) {
    case Cbs:    break;
    case Cws:    setval1 = n; xst = Scmt; xst2 = Sset0; break;
    case Cnl:    setval1 = n; xst = Sset0; break;
    default:     if (Ctab[c] == 0 || c == '\r') serror(n,"nonprintable char '%s' : use \\xx",chprint(c));
    }
    if (setval1) {
      set = addset(lno,setgrp,buf+setnam0,setnlen,buf+setval0,setval1-setval0);
      setlns[set] = lno;
    }
  break;

  // tokens
  case Stoknam0:
    switch (t) {
    case Cnl:    break;
    case Calpha: varnam0 = n; varnam1=0; xst = Svarnam; break;
    case Chsh:   xst = Scmt; xst2 = Stoknam0; break;
    case Cws:    xst = Stoknam1; break;
    default :    serror(n,"expected token, found '%s'",chprint(c));
    }
  break;

  case Stoknam1:
    if (t == Calpha) { toknam0 = n; toknlen = 0; xst = Stoknam; }
    else if (t == Cnl) xst = Stoknam0;
    else if (t != Cws) serror(n,"expected token, found '%s'",chprint(c));
    break;

  case Stoknam:
    switch (t) {
    case Calpha: case Cnum: break;
    case Cws: toknlen = n - toknam0; xst = Stokgrp; break;
    case Cnl: toknlen = n - toknam0; xst = Stoknam0; break;
    case Chsh:toknlen = n - toknam0; xst2 = Stoknam0; xst = Scmt; break;
    default: serror(n,"expected toknam, found %c",c);
    }
    if (toknlen && xst != Stokgrp) tk = addtok(lno,buf+toknam0,toknlen,0);
  break;

  case Stokgrp:
  grp = 255;
  switch (t) {
    case Cws: break;
    case Chsh: xst2 = Stoknam0; xst = Scmt; break;
    case Cnl:  xst = Stoknam0; break;
    case Cnum: grp = c - '0';
               xst = Shrtok0;
               break;
    default: serror(n,"expected toknam, found %c",c);
    }
    tk = addtok(lno,buf+toknam0,toknlen,grp == 255 ? 0 : grp);
  break;

  case Shrtok0:
  switch (t) {
    case Cws: break;
    case Chsh: xst2 = Stoknam0; xst = Scmt; break;
    case Cnl:  xst = Stoknam0; break;
    default:   toknam0 = n; toknlen = 0; xst = Shrtok;
  }
  break;

  case Shrtok:
  switch (t) {
    case Cws:
    case Chsh: toknlen = n - toknam0; xst2 = Stoknam0; xst = Scmt; break;
    case Cnl:  toknlen = n - toknam0; xst = Stoknam0; break;
    default:   if (is_print(c) == 0) serror(n,"token %u hr char '%s' is not printable",tk,chprint(c));
  }
  if (toknlen) {
    if (toknlen > 4) serror(n,"token %u hr name len %u exceeds 4",tk,toknlen);
    c = buf[toknam0];
    if (toknlen == 1 && hrdups[c]) serror(n,"token %u hr name '%c' previously defined at line %u",tk,c,hrdups[c]);
    hrdups[c] = lno;
    memcpy(hrtoks + tk * 4,buf+toknam0,toknlen);
    tokhrctl[tk] = toknlen;
  }
  break;

  case Sact0:
    switch (t) {
    case Cnl: break;
//    case Cws: break;
    case Cnum:   actena = (c == passc); n++; break;
    case Cbtk:   actena = !disabled[c2]; n += 2; break;
    case Chsh:   xst = Scmt; xst2 = Sact0; break;
    case Cdot:   actself = 1; break;
    case Calpha: actnam0 = n; actnam1=0; xst = Sact1; break;
    default:     serror(n,"expected actnam, found '%s'",chprint(c));
    }
  break;

  case Sact1: // act name
    switch(t) {
    case Calpha: case Cnum: break;
    case Cnl: case Cws: case Chsh:
      actnam1 = n;
      slen = actnam1 - actnam0;
      if (slen == 5 && memeq(buf+actnam0,"table",5)) {
        getvar(buf,actnam0,actnam1);
        if (nstate == 0) addstates(lno,buf,n);
        xst = Sstate0;
      } else {
        vrb("new act %.*s",slen,buf+actnam0);
        if (actena) act = addact(lno,buf+actnam0,slen);
        else act = hi16;
        if (actself) actrefs[act] = lno;
        actself = 0;
      }
      break;
    default: serror(n,"expected actnam, found %c",c);
    }
    if (actnam1 == 0) break;
    else if (xst == Sstate0) break;
    else if (t == Cnl) xst = Sact20;
    else { xst2 = Sact20; xst = Scmt; }
  break;

  case Sact20: // act code
    xst2 = Sact20;
    switch (c) {
    case '1':  if (pass2) xst = Scmt; else xst = Sact21; break;
    case '2':  if (pass1) xst = Scmt; else xst = Sact21; break;
    case ' ':
    case '.':  xst = Sact21; break;
    case '#':  xst = Scmt; break;
    case '\n': if (actena) addactval(lno,act,"\n",1);
               xst = Sact0; actena = 1; break;
    case 0:    serror(n,"ln %u unexpected eof",lno);
    default:   serror(n,"expected code prelude, found %c", c);
    }
  break;

  case Sact21:
    switch(t) {
    case Cws:  xst = Sact2; break;
    case Cnl:  xst = Sact20; break;
    case Chsh: xst2 = Sact20; xst = Scmt; break;
    case Calpha: case Cnum: actval0 = n; actval1 = 0; xst = Sact3; break;
    default:
      if (c == '/' || c == '{' || c == '}') { actval0 = n; actval1 = 0; xst = Sact3; }
      else serror(n,"expected code prelude, found '%c'", c);
    }
  break;

  case Sact2:
    switch (c) {
    case '\n': xst = Sact20; break;
    case 0:    serror(n,"ln %u unexpected eof",lno);
    default:   if (Ctab[c] == 0) serror(n,"action %.*s nonprintable char '%s'",slen,buf+actnam0,chprint(c));
               actval0 = n; actval1 = 0; xst = Sact3;
    }
  break;

  case Sact3:
    switch (c) {
    case '\n': slen = n - actval0;
               if (actena) addactval(lno,act,buf+actval0,slen);
               actval0 = actval1 = 0;
               xst = Sact20;
               break;
    case '\r': serror(n,"action %.*s nonprintable char '%s'",slen,buf+actnam0,chprint(c));
    case 0:    serror(n,"ln %u unexpected eof",lno);
    default:   if (Ctab[c] == 0) serror(n,"action %.*s nonprintable char '%s'",slen,buf+actnam0,chprint(c));
    }
  break;

  // table
  case Sstate0: // start of state
    if (tp - transtab >= Translen) serror(n,"exceeding %u patterns",Translen);
    switch (t) {
    case Cws:
    case Cnl:    break;
    case Chsh:   if (c2 == '#') goto eof;
                 else { xst = Spcmt; break; }
    case Calpha: idnam0 = n; idnam1 = 0; xst = Sstate1; break;
    case Cnum:
    default:     serror(n,"expected state name, found %c",c);
    }
    break;

  case Sstate1: // in state
    switch (t) {
    case Cast:
    case Cws:
    case Cdot:
    case Cnl: idnam1 = n; break;

    case Calpha: case Cnum: break;
    default:   serror(n,"unexpected char '%s' in state",chprint(c));
    }
    if (!idnam1) break;
    idlen = idnam1-idnam0;
    if (idlen + 1 >= Stnam) serror(n,"state %.*s: name exceeds len %u",Stnam,buf+idnam0,Stnam);
    st0 = getstate(buf,idnam0,idlen);
    if (st0 == nstate) {
      serror(n,"state '%.*s' unknown",idlen,buf+idnam0);
    }

    memcpy(stnam,buf+idnam0,idlen);
    stnam[idlen] = 0;

    tp2 = stdefs[st0];
    if (tp2) serror(idnam0,"state %s already defined at %u",stnam,tp2->ln);
    stdefs[st0] = tp;
    svrb(n,"new state %s pat %u",stnam,(ub4)(tp - transtab));

    streach = statereach + st0 * Nstate;

    if (t == Cast) {
      strefs[st0] = tp;
      statereach[st0] |= 1;
      if (c2 == '*') { statereach[st0] = 0xff; n++; }
    }  else if (t == Cdot && c2 == 'N') tp->setN = 1;

    idnam0 = idnam1 = 0;
    patno = 0;
    otherln = 0;
    tp->st0 = st0;
    tp->tk = nltok;
    if (t == Cnl) xst = Spat0; else xst = Sstate2;
  break;

  case Sstate2: // after state
    switch (t) {
    case Cws: break;
    case Cnl: xst = Spat0; break;
    case Chsh:
      idnam0 = n+1;
      tp->cmt = buf+n+1;
      xst = Sstate3;
      break;
    case Calpha: case Cnum:
      idnam0 = n; idnam1 = 0; xst = Sstate4;
    break;
    default: serror(n,"unrecognised char '%s' after state",chprint(c));
    }
  break;

  case Sstate3:
    if (t == Cnl) {
      idnam1 = n;
      tp->cmtlen = idnam1 - idnam0;
      xst = Spat0;
    } else if (c == 0) serror(n,"unexpected eof in state %s",stnam);
    else if (c == '\r' || Ctab[c] == 0) serror(n,"state %s nonprintable char '%s' in comment",stnam,chprint(c));
  break;

  case Sstate4:
    if (t == Cnl) {
      idnam1 = n;
      while (idnam1 > idnam0 && buf[idnam1-1] == ' ') idnam1--;
      idlen = idnam1 - idnam0;
      stdescs[st0] = buf+idnam0;
      stdesclens[st0] = idlen;
      xst = Spat0;
    } else if (c == 0) serror(n,"unexpected eof in state %s",stnam);
    else if (c == '\r' || Ctab[c] == 0) serror(n,"state %s nonprintable char '%s' in comment",stnam,chprint(c));
  break;

  case Spat0: // new state or next pat
    if (tp - transtab >= Translen) serror(n,"exceeding %u patterns",Translen);
    tp->tk = tp->tkhid = Ntok-1;
    tp->act = Nact - 1;

    switch (t) {
    case Cws:    xst = Spat1; break;
    case Cnum:   if (c == '1' || c == '2') {
                   if (c != passc) xst = Spcmt;
                 }
                 break;
    case Cbtk:   if (disabled[c2]) xst = Spcmt; else n++; break;
    case Cnl:    break;
    case Chsh:   if (c2 == '#') goto eof;
                 else { xst = Spcmt; break; }
    case Calpha: idnam0 = n;   idnam1 = 0; xst = Sstate1; break;
    default:     if (c == 0) xst = Seof; else serror(n,"expected pattern, found '%s'",chprint(c));
    }

    if (pass == 1 && patno && xst == Sstate1 && otherln == 0) {
      tp->st0 = st0; // add 'ot Err'
      tp->st = st0;
      tp->ln = lno;
      tp->patno = patno++;
      vrb("state %s other pat %u ln %u",stnam,tp->patno,lno);
      strcpy(tp->pat,"ot");
      tp->patlen = 2;
      tp->iserr = 1;
      tp++;
      tp->tk = tp->tkhid = Ntok-1;
      tp->act = Nact - 1;
    }
  break;

  case Spat1: // before pat
    idnam1 = 0;
    switch (t) {
    case Cws:  break;
    case Cnl:  xst = Spat0; break;
    case Cbtk: idnam0 = n+1; idnam1 = 0; xst = Spat2; if (c2 != '`') tp->dowhile = 1; break;
    case Chsh: if (c2 != '#') xst = Spcmt;
               else {
                 idnam0 = n + 1; idnam1 = 0; xst = Spat2;
               }
               break;

    default:   idnam0 = n;   xst = Spat2;
    }
  break;

#if 0
  case Spat1a:
    u1v = atox1(c);
    if (u1v > 0xf) serror(n,"expected \xx, found %s",chprint(c));
    u2v = u1v;
    xst = Spat1b;
  break;

  case Spat1b:
    u1v = atox1(c);
    if (u1v > 0xf) serror(n,"expected \xx, found %s",chprint(c));
    u2v = u2v << 4 | u1v;
    xst = Spat2;
    tp->st0 = tp->st = st0;
    tp->patlen = 1;
    tp->pat[0] = u2v;
  break;
#endif

  case Spat2: // in pat
    switch (t) {
    case Calpha: case Cnum: break;
    case Cdot: case Cdq: case Ccomma: case Chsh: break;
    case Cws:
    case Cnl: idnam1 = n; break;
    default: break;
    }
    if (idnam1 == 0) break;

    tp->st0 = st0;
    tp->st = st = st0;
    tp->ln = lno;
    tp->patno = patno++;
    idlen = idnam1 - idnam0;
    if (idlen + 1 >= Patlen) serror(n,"state %s: pattern exceeds len %u",stnam,Patlen);

    memcpy(tp->pat,buf+idnam0,idlen);
    tp->patlen = idlen;
    tp->pat[idlen] = 0;
    if (strcmp(tp->pat,"ot") == 0) {
      if (otherln) serror(n,"duplicate other, first at ln %u",otherln);
      otherln = lno;
    }
    idnam0 = idnam1 = 0;
    if (t == Cnl) {
      xst = Spat0;
      tp++;
    } else xst = Snxstate0;
  break;

  case Snxstate0:
    switch (t) {
    case Cws:    break;
    case Chsh:   tp->st = st0; tp++; xst = Spcmt; break;
    case Cdot:   tp->st = st0; xst = Stoken0; break;
    case Cast:   tp->st = st0; xst = Stoken0; tp->actstate = 1; break;
    case Cmin:   idnam0 = n+1; idnam1 = 0; tp->dobt = 1;  xst = Snxstate1; break;
    case Calpha: idnam0 = n;   idnam1 = 0; xst = Snxstate1; break;

    case Cnl:    serror(n,"incomplete pattern for %s",stnam);
    default:     serror(n,"state %s unexpected char '%s' for next state",stnam,chprint(c));
  }
  break;

  case Snxstate1: // in  next state
    switch (t) {
    case Calpha: case Cnum: case Cmin: break;
    case Cws:
    case Chsh:
    case Cnl:
      idnam1 = n;
      idlen = idnam1 - idnam0;
      if (idlen + 1 >= Stnam) serror(n,"state %s: name exceeds len %u",stnam,Stnam);
      if (idlen == 3 && memeq(buf+idnam0,"EOF",3)) {
        tp->eof = 1;
        break;
      }
      st = getstate(buf,idnam0,idlen);
      if (st == st0 && tp->dobt == 0 && tp->actstate == 0) serror(n,"next state %s identical to current",stnam);
      else if (st == nstate) {
        serror(n,"state '%.*s' unknown",idlen,buf+idnam0);
      }
      if (strefs[st] == nil) strefs[st] = tp;
      streach[st] |= 1;
      tp->st = st;
      break;
    default: serror(n,"unexpected char '%s'",chprint(c));
    }
    if (idnam1) {
      idnam0 = idnam1 = 0;
      if (t == Cnl) {
        xst = Spat0;
        tp++;
      } else if (t == Chsh) {
        xst = Spcmt;
        tp++;
      } else {
        xst = Stoken0;
      }
    }
  break;

  case Stoken0:
    switch (t) {
    case Cws:    break;
    case Cnum:   if (c > '3') serror(n,"invalid pass %c",c); Fallthrough
    case Calpha: idnam0 = n; idnam1 = 0; xst = Stoken1; break;
    case Cdot:   xst = Spatact0; break;
    case Chsh:   xst = Spcmt; tp++; break;
    case Cnl:    xst = Spat0; break;
    default:     serror(n,"unexpected char %s",chprint(c));
  }
  break;

  case Stoken1: // in token
    switch (t) {
    case Calpha: case Cnum: case Cdot: break;
    case Cws:
    case Chsh:
    case Cnl: idnam1 = n; break;
    default:  serror(n,"unexpected char %s",chprint(c));
    }
    if (!idnam1) break;
    c=buf[idnam0];
    if (c == '1' || c == '2' || c == '3') { // pass n only
      idnam0++;
      if (buf[idnam0] == '.') idnam0++;
      tokeep = (c == passc);
    } else tokeep = 1;
    slen = idnam1 - idnam0;
    if (slen == 0) serror(idnam0,"pat %s empty token name",tp->pat);
    else if (slen >= Toknam) serror(idnam0,"pat %s token name '%.*s'exceeds %u",tp->pat,Toknam,buf+idnam0,Toknam);

    if (buf[idnam1-1] == '.') { tp->hrtok = 1; slen--; }
    if (slen == 0 || (slen == 1 && buf[idnam0] == '.') ) serror(idnam0,"pat %s empty token name",tp->pat);

    kw = getkwd(buf+idnam0,slen);
    if (kw < nkwd) tk = kw;
    else tk = gettoken(lno,buf,idnam0,slen);
    tkrefs[tk] = tp->ln;
    idnam0 = idnam1 = 0;
    if (tokeep) {
      if (st) serror(lno|Lno,"%u next state %.*s while producing token %.*s",lno,stlens[st],states[st],toklens[tk],toks[tk]);
      tp->tk = tk;
      tkpass[tk] = passc;
    } else tp->tkhid = tk;

    if (t == Cnl) {
      xst = Spat0;
      tp++;
    } else if (t == Chsh) {
      xst = Spcmt; tp++;
    } else {
      xst = Spatact0;
    }
  break;

  case Spatact0:
    switch (t) {
    case Cws:    break;
    case Cnum:   if (c != passc) skipact = 1;
                 break;
    case Calpha: idnam0 = n; idnam1 = 0; xst = Spatact1; break;
    case Cbs:    if (c2 == '\n') { n++; } break;
    case Chsh:   xst = Spcmt; tp++; break;
    case Cnl:    xst = Spat0; tp++; break;
    case Cdot:
    case Cbtk:   actval0 = n+1; actval1=0; xst = Scode; break;
    default:     serror(n,"expected action, found %c",c);
    }
  break;

  case Spatact1:
    switch (t) {
    case Calpha: case Cnum: break;
    case Cws:
    case Chsh:
    case Cbs:
    case Cnl:  idnam1 = n; break;
    default:   actval0 = idnam0; xst = Scode;
    }
    if (!idnam1) break;
    slen = idnam1 - idnam0;
    svrb(n,"get act '%.*s'",slen,buf+idnam0);
    act = getact(lno,buf+idnam0,slen);
    if (skipact == 0) {
      slen = actvlens[act];
      codlen = tp->codlen;
      if (codlen + slen + 32 >= Codlen) serror(n,"pat '%s' act %.*s code len %u+%u exceeds %u",tp->pat,actnlens[act],acts[act],codlen,slen + 32,Codlen);
      code = actvals + act * Actlen;
      codlen += mysnprintf(tp->code,codlen,Codlen,"%.*s// %.*s",codlen,"\n",actnlens[act],acts[act]);
      if (ena_lno) codlen += mysnprintf(tp->code,codlen,Codlen," ln %u",actlns[act]);
      tp->code[codlen++] = '\n';
      memcpy(tp->code + codlen,code,slen);
      tp->codlen = codlen + slen;
    }
    else skipact = 0;
    actrefs[act] = lno;
    actcnts[act]++;
    idnam0 = idnam1 = 0;
    if (t == Cnl) { tp++; xst = Spat0; }
    else if (t == Cbs && c2 == '\n') {
      n++;
      xst = Spatact0;
    } else xst = Sccmt;
  break;

  case Scode:
    if (t == Cnl || t == Cbtk || (t == Chsh && c2 == '#')) actval1 = n;
    else if (c == '\\' && c2 == '\n') actval1 = n;
    else if (c == 0) serror(n,"unexpected eof in state %s",stnam);
    else if (c == '\r' || Ctab[c] == 0) serror(n,"state %s nonprintable char '%s' in code",stnam,chprint(c));

    if (actval1 == 0) break;

    slen = actval1 - actval0;
    if (slen == 0) serror(n,"empty code for pat '%s'",tp->pat);
    while (actval1 > actval0 && buf[actval1-1] == ' ') actval1--;
    slen = actval1 - actval0;
    if (slen == 0) serror(n,"empty code for pat '%s'",tp->pat);
    codlen = tp->codlen;
    if (codlen + slen + 4 >= Codlen) serror(n,"pat '%s' code len %u exceeds %u",tp->pat,codlen + slen,Codlen);

    if (skipact == 0) {
      if (codlen) tp->code[codlen++] = ' ';
      for (i = 0; i < slen; i++) {
        c = buf[actval0+i];
        if (c == '#') {
          if (i < slen - 1 && buf[actval0+i+1] == '#') { i++; tp->code[codlen++] = c; }
          else break;
        } else if (c == '$' && i < slen - 1) {
          i++;
          c = buf[actval0+i];
          if (c == 'S') { // state
            codlen += mysnprintf(tp->code,codlen,Codlen,"\"%s\"",stnam);
          } else if (c == 'P') { // pat
            codlen += mysnprintf(tp->code,codlen,Codlen,"\"%s\"",chprints(tp->pat,32));
          } else if (c == '!') { // error
            tp->actstate = 1;
          } else {
            tp->code[codlen++] = '$';
            tp->code[codlen++] = c;
          }
        } else tp->code[codlen++] = c;
      }
      tp->codlen = codlen;
    } else skipact = 0;
    actval0=actval1=0;
    if (t == Cnl) { xst = Spat0; tp++; }
    else if (t == Chsh) { xst = Sccmt; }
    else if (t == Cbtk) { actval0 = n + 1; xst = Spatact0; }
    else { xst = Spatact0; n++; }
  break;

  case Spcmt:
    if (t == Cnl) {
      xst = Spat0;
    } else if (c == 0) xst = Seof;
  break;

  case Sccmt:
    if (c == '\\' && c2 == '\n') {
      n++;
      actval1=0;
      xst = Spatact0;
    } else if (t == Cnl) {
      tp++;
      xst = Spat0;
    } else if (c == 0) serror(n,"unexpected eof in state %s",stnam);
  break;

  case Seof:
    if (otherln == 0) break;
    tp->st0 = st0; // add 'ot Err'
    tp->st = st0;
    tp->ln = lno;
    tp->patno = patno++;
    strcpy(tp->pat,"ot");
    tp->patlen = 2;
    tp->iserr = 1;
    tp++;
  break;

  } // switch state
  n++;
  } while (1);

  eof:

  if (svfpos[Sv_table] == 0) serrorfln(FLN,Lno,"missing table section");

  tp->st0 = nstate;

  if (nstate == 0) serrorfln(FLN,Lno,"no states");
  info("read spec from %s",fname);

  showcnt("2keyword",nkwd);
  showcnt("2xtoken",nltok);
  showcnt("2token",nkwd + nltok);
  showcnt("2bltin",nblt);

  if (*specdate) snprintf(specxdate,64,"%.32s - %s",specdate,fmtdate(specdtim,specdmin));
  else strcpy(specxdate,fmtdate(specdtim,specdmin));

  ub4 transcnt = (ub4)(tp - transtab);

  if (transcnt == 0) serror(lno|Lno,"no transitions for %u states",nstate);
  else if (transcnt == 1) serror(lno|Lno,"only one transition for %u states",nstate);
  showcnt("2pattern",transcnt);
  transtablen = transcnt;

  ub1 haveact=0;
  for (tk = ntok; tk < nltok-1; tk++) {
    if (tkrefs[tk] == 0) swarn(Lno|toklnos[tk],"token %.*s unreferenced",toklens[tk],toks[tk]);
    haveact += tkacts[tk];
  }
  if (msgerrcnt()) return 1;

  for (tk = 0; tk < ntok; tk++) {
    if (tkrefs[tk] == 0) swarn(Lno,"token %.*s unreferenced",toklens[tk],toks[tk]);
  }
  if (msgerrcnt()) return 1;

  for (st0 = 0; nstate > 1 && st0 < nstate; st0++) {
    tp1 = stdefs[st0];
    tp2 = strefs[st0];
    if (!tp1 && !tp2) err(nil,st0,"state %u unused",st0);
    else if (!tp1) err(tp2,st0,"state %u undefined",st0);
    else if (!tp2) err(tp1,st0,"state %u unreferenced",st0);
//    else if (tp1 == tp2) err(tp1,st0,"state self-references: def %u ref %u",tp1->ln,tp2->ln);
  }
  if (msgerrcnt()) return 1;

  for (act = 0; act < nact; act++) {
    if (actrefs[act] == 0) swarn(actlns[act]|Lno,"action %.*s unreferenced",actnlens[act],acts[act]);
  }
  if (msgerrcnt()) return 1;

  closesets();

  bool change;

#if 0
  for (st00 = 0; st00 < nstate; st00++) {
    if (statereach[st00] == 0xff) continue;
    memset(stcover,0,sizeof(stcover));
    stcover[st00] = 1;
    do {
      change = 0;
      for (st0 = 0; st0 < nstate; st0++) {
        if (stcover[st0] == 0) continue;

        streach = statereach + st0 * Nstate;
        for (st = 0; st < nstate; st++) {
          if (streach[st] & 1 && stcover[st] == 0) { stcover[st] = 1; change = 1; }
        }
      }
    } while (change);

    for (st = 0; st < nstate; st++) {
      if (stcover[st] == 0) swarn(stlnos[st]|Lno,"state %-8.*s unreachable from %.*s ln %u",stlens[st],states[st],stlens[st00],states[st00],stlnos[st00]);
    }
//    if (msgerrcnt()) return 1;
  }
#endif

  for (n = 0; n < transcnt; n++) {
    tp = transtab+n;
    if (tp->patlen == 0) err(tp,tp->st0,"empty pattern #%u",n);
    else if (tp->pat[0] == 0) err(tp,tp->st0,"empty pattern with len %u",tp->patlen);
    if (tp->ln == hi16 || tp->ln == 0) err(tp,nstate,"invalid entry at %u",n);
  }
  if (msgerrcnt()) return 1;

  for (act = 0; act < nact; act++) {
    cnt = actcnts[act];
    if (cnt > 2) info("%u %.*s",cnt,actnlens[act],acts[act]);
  }

  return 0;
}

// static ub4 trclvl;

static void wrfhdr(struct bufile *fp,ub1 addinfo)
{
  char buf[256];

  myfprintf(fp,"   generated by genlex %s %s\n\n",version,fmtdate(globs.prgdtim,globs.prgdmin));
  myfprintf(fp,"   from %s %s %s %s\n",
    specname,specversion,specxdate,speclang);

  mysnprintf(buf,0,256,"code %s  tokens %s",yesno(!omitcode),yesno(!omittoken));

  if (addinfo) myfprintf(fp,"   options: %s\n */\n",buf);

  if (addinfo > 1) {
    myfprintf(fp,"static const char specnam[] = \"%s\";\n\n",specname);
    myfprintf(fp,"static const char lexinfo[] = \"%s %s  %s %s  %s\";\n\n",specname,specversion,specxdate,speclang,buf);
  }
}

static ub4 wrhsh(struct bufile *lfp,struct bufile *sfp,cchar *pfx,cchar *names[],ub1 *lens,ub1 *map,ub2 cnt,ub1 *hsh,ub2 hshlen,bool dopool)
{
  #define Spool (Nkwd * 8)
  ub2 i,len,x,eoflen;
  ub4 hc=0;
  ub2 spos=0;
  char spool[Spool];
  char buf[512];
  ub2 blen = 512,bpos=0;
  ub2 sposs[Nkwd];
  char pfx0 = *pfx++;
  char pfx1 = upcase(*pfx);
  ub2 pfxlen = strlen(pfx);
  cchar *mpfx = map ? "t" : "";

 if (dopool && sfp->top) {
  if (Ctab[pfx[pfxlen-1]] == Cnum) pfxlen--;
  memset(spool,' ',Spool);
  for (i = 0; i < cnt; i++) {
    spos = align4(spos);
    len = lens[i];
    if (spos + len + 1 >= Spool) serror(Lno,"%u/%u strpool len exceeds %u",i,cnt,Spool);
    memcpy(spool + spos,names[i],len);
    sposs[i] = spos;
    spos += len;
  }
  sposs[cnt] = spos;
  eoflen = mysnprintf(spool + spos,0,64,"unknown_%.*s",pfxlen,pfx);
  spos += eoflen + 1;

  myfprintf(sfp,"static const char %s%.*snampool[%3u] = \"%.*s\\0\";\n\n",mpfx,pfxlen+1,pfx,spos,spos,spool);

  hc = hashalstr(spool,spos,hshseed);
  info("hsh %s pool len %u %x seed %x '%.*s'",pfx,spos-1,hc,hshseed,min(spos,32),spool);

  myfprintf(sfp,"static const ub2  %s%.*snamposs[%3u] = { ",mpfx,pfxlen,pfx,cnt+1);
  for (i = 0; i <= cnt; i++) {
    myfprintf(sfp,"%s%u",i ? "," : "",sposs[i]);
  }
  myfputs(sfp," };\n\n");

  myfprintf(sfp,"static const ub1  %s%.*snamlens[%3u] = { ",mpfx,pfxlen,pfx,cnt+1);
  for (i = 0; i < cnt; i++) {
    myfprintf(sfp,"%s%u",i ? "," : "",lens[i]);
  }
  myfputs(sfp," };\n\n");

 } // do sfp

  myfprintf(lfp,"#define %c%shshmsk %u\n",pfx1,pfx+1,hshlen-1);
  myfprintf(lfp,"#define %c%shshbit  %u\n\n",pfx1,pfx+1,msb(hshlen));
  myfprintf(lfp,"#define x %c99_count\n\n",pfx0);

  myfprintf(lfp,"static const ub1 %shsh[%u] = {\n  ",pfx,hshlen);
  bpos = 0;
  for (i = 0; i < hshlen; i++) {
    x = hsh[i];
    if (x < cnt) {
      myfprintf(lfp,"%-3u",x);
      bpos += mysnprintf(buf,bpos,blen," %.*s",lens[x],names[x]);
    } else {
      myfputs(lfp,"x  ");
      buf[bpos++] = '.';
    }
    if (i < hshlen-1) myfputc(lfp,',');
    if (( i & 15) == 15) {
      myfprintf(lfp," // %.*s\n  ",bpos,buf);
      bpos = 0;
    }
  }
  if (bpos) myfprintf(lfp," // %.*s\n",bpos,buf);
  myfputs(lfp,"};\n");

  myfputs(lfp,"#undef x\n\n");
  return hc;
}

static cchar *fmtname(ub2 len,cchar *p)
{
  static char buf[64];
  char *q = buf;

  while (len) { *q++ = (*p == '-' ? '_' : *p); p++; len--; }
  *q = 0;
  return buf;
}

static cchar *strupper(cchar *p)
{
  static char buf[64];
  char *q = buf;

  while (*p) { *q++ = (*p < 'a' ? *p : *p - 0x20); p++; }
  *q = 0;
  return buf;
}

static void printsets(struct bufile *f,ub1 mode)
{
  ub2 s,c,bit,grp;
  enum Ctype t;
  cchar *nam;

  if (mode == 2) myfputs(f,"/*\n");

  for (s = 0; s < nset; s++) {
    if (setcase[s]) continue;
    nam = strupper(setnams + s * Snam);
    if (mode) {
      bit = setbits[s];
      grp = setgrps[s];
      if (mode == 1) myfprintf(f,"#define %s %-2u // ",nam,bit);
      else if (mode == 2) myfprintf(f," %-3s %-2u ",nam,bit);
      if (grp == 1) myfputc(f,'+');
      else if (grp == 2) myfputc(f,'-');
      myfputs(f," ' ");
      if (grp == 2) {
        myfputs(f,chprint(setval0s[s])); myfputc(f,' ');
      } else {
        for (c = 0; c < 128; c++) {
          if (setgrps[s] == 1) {
            if ( (cctab[c] & bit) == 0) continue;
          } else if (ctab[c] != bit) continue;
          t = Ctab[c];
          if (t && c >= ' ') {
            myfputc(f,c); if (t != Calpha && t != Cnum) myfputc(f,' ');
          } else {
            myfputs(f,chprint(c)); myfputc(f,' ');
          }
        }
      }
      myfputs(f,"'\n");
    } else myfprintf(f,"#undef %s\n",nam);
  }
  if (mode == 2) myfputs(f,"*/\n");
}

static void printtrans(struct bufile *f)
{
  ub2 st0,st,tk,act,ndx;
  struct trans *tp;
  cchar *p,*pp;

  myfputs(f,"\n#if 0\n\n--- transcript of parsed spec ---\n\n");

  st0 = transtab->st0;
  for (ndx = 0; ndx < transtablen; ndx++) {
    tp = transtab + ndx;
    if (tp->st0 != st0) {
      if (st0 == nstate) break;
      st0 = tp->st0;
      myfprintf(f,"\n%.*s\n  ",stlens[st0],states[st0]);
    }
    p = tp->pat;
    while (*p) {
      switch (*p) {
        case '"': pp = "\\\""; break;
        case '\'': pp = "\\\'"; break;
        case '\\': pp = "\\\\"; break;
        default: pp = chprint(*p);
      }
      myfputs(f,pp);
      p++;
    }
    st = tp->st;
    tk = tp->tk;
    if (tp->iserr) myfputs(f,"\tErr");
    else if (st == st0) myfputs(f,"\t.\t");
    else myfprintf(f,"\t%.*s\t",stlens[st],states[st]);
    if (tk == nltok) myfputs(f,".\t");
    else myfprintf(f,"%.*s\t",toklens[tk],toks[tk]);
    if ( (act = tp->act) < nact) {
      myfprintf(f,"%.*s\t",actnlens[act],acts[act]);
    }
    myfputs(f,"\n  ");
  }

  myfputs(f,"\n#endif\n");
}

static int filediff(cchar *fnam,cchar *buf,ub2 blen)
{
  struct myfile mf;
  ub4 len,n0,n=0,nn;
  cchar *p;

  // ub8 binmtim = osfiltim(globs.prgnam);

  if (readfile(&mf,shdrname,0,hi16)) return -1;
  else if (mf.exist == 0) {
    info("%s does not exist yet, creating",fnam);
    return 1;
  }

  len = mf.len;
  p = mf.buf;

  if (len < 16) return 1;

  // if (binmtim > mf.mtime) return 1;

  while (n < len && p[n] != '@') n++;
  if (n + 1 >= len) {
    info("%s has no start diff marker, creating",fnam);
    return 1;
  }
  n++; n0 = n;
  while (n < len && p[n] != '@') n++;
  if (n + 1 >= len) {
    info("%s has no end diff marker, creating",fnam);
    return 1;
  }
  nn = n - n0;
  if (nn != blen) {
    info("%s will differ in len %u vs %u",fnam,nn,blen);
    return 1;
  }
  for (n = 0; n < nn; n++) {
    if (p[n+n0] != buf[n]) {
      info("%s will differ at pos %u '%c' vs '%c'",fnam,n,p[n+n0],buf[n]);
      return 1;
    }
  }
  info("%s will equal for len %u : skipped",fnam,nn);
  return 0;
}

static ub2 wrcode(char *buf,ub2 bpos,ub2 blen,ub2 len,cchar *ap,ub2 ln)
{
  ub2 n;
  cchar *p;

  if (bpos + 32 >= blen) serror(ln|Lno,"exceeding buf for %.*s at %u",len,ap,bpos);

  while (len) {
    p = memchr(ap,'`',len);
    if (p && p < ap + len - 1) {
      n = p - ap;
      bpos += mysnprintf(buf,bpos,blen,"%.*s",n,ap);
      switch (p[1]) {
      case 'L': bpos += mysnprintf(buf,bpos,blen,"%u",ln); break;
      case '`': bpos += mysnprintf(buf,bpos,blen,"`"); break;
      default:  bpos += mysnprintf(buf,bpos,blen,"lx_%c",p[1]); // goto `slit
      }
      ap = p + 2;
      len -= n + 2;
    } else {
      bpos += mysnprintf(buf,bpos,blen,"%.*s ",len,ap);
      break;
    }
  }
  if (len > 48) buf[bpos++] = '\n';
  return bpos;
}

static void wrhshlut(struct bufile *fp,cchar *pfx,ub1 *lut)
{
  char buf[2048];
  ub2 c,cnt1=0,cnt2=0,pos,len=2048;
  ub1 x;

  pos = mysnprintf(buf,0,len,"static const unsigned char %shshlut[256] = {\n  ",pfx);
  for (c = 0; c < 256; c++) {
    if (c) buf[pos++] = ',';
    if (c && (c & 0xf) == 0) pos += mysnprintf(buf,pos,len,"\n  ");
    x = lut[c];
    if (x & 0xf) cnt1++;
    if (x & 0xf0) cnt2++;
    pos += mysnprintf(buf,pos,len,"%3u",x);
  }
  myfprintf(fp,"%.*s\n}; // %u,%u\n\n",pos,buf,cnt1,cnt2);
}

static void wrenum(struct bufile *fp,ub1 *lens,cchar **nams,char *name,ub2 cnt,ub4 upad)
{
  ub2 n;
  sb4 pad = -upad-1;
  cchar *nam,*pfx="";

  if (*name > 'Z') pfx = "_";

  pad = min(pad,-8);
  if (cnt == 0) {
    myfprintf(fp,"enum %s%s { %c99_count = 1 };\n\n",packed8,name,*name);
    return;
  }

  myfprintf(fp,"enum %s%s {\n ",packed8,name);
  for (n = 0; n < cnt; n++) {
    nam = nams[n];
    if (strcmp(nam,"false") == 0) nam = "fals";
    myfprintf(fp," %s%c%*.*s = %3u,",pfx,*name,pad,lens[n],nam,n);
    if ( (n & 7) == 7) myfputs(fp,"\n ");
  }
  if (n & 7) myfputs(fp,"\n ");
  myfprintf(fp," %c%*s = %2u\n};\n\n",*name,pad,"99_count",cnt);
}

static void wrhshshift(struct bufile *fp,cchar *infix,ub2 shift)
{
  if (shift) myfprintf(fp,"#define %shshshift(h) h >> %u\n",infix,shift);
  else myfprintf(fp,"#define %shshshift(h) h\n",infix);
}

static int wrfile(void)
{
#define Buflen 4096
  static char buf[Buflen];
  ub4 blen = Buflen;
  ub2 bpos = 0,bpos2=0;

  char buf2[256];
  ub4 blen2 = 256;

  int rv;

  ub2 tkpos=0;
  ub2 tklen = 512;
  char tkbuf[512];

  char st0nam[Stnam];

  ub2 lno=0,lnx;

  ub1 st=0,st0;
  ub1 tk=0;

  cchar *nst;
  cchar *p;
  ub1 t;
  ub1 grp;
  ub2 len,inclen,minlen,nlen;
  ub2 c;
  ub2 ccnt,tcnt,ucnt,ocnt,rcnt,totcnt;

  cchar *nam;

  struct trans *tp,*tp0=nil;
  ub2 ttndx,ttndx2,ttend=0,ttany=0;
  ub2 i,i2;
  ub1 s,loopc=0,his,*sp;
  ub2 ifcnt;
  ub1 x1;
  ub4 hc;
  ub1 ec;
  ub2 tktabcnt;
  ub2 hitktab;
  enum Ctl ctl,prvctl,*cp;
  bool dotswitch,haveany,isany;
  ub2 act;
  ub2 codlen;
  cchar *snam;
  ub1 ctbl[256];
  ub1 ttbl[256];
  ub2 ctltab[256];

  ub2 spos=0,sposz=0;
  char spool[Spool];
  ub2 sposs[Nstate];
  ub1 lens[Nstate];

  ub2 eoflen;
  ub2 set,bit;
  bool havetvar,haveuvar,haveeof=0,dobt=0,havetk;

  static struct bufile sfp;
  ub4 shdrbuf = nkwd * 32U + nblt * 32U + ndun * 32U + 0x4000;
  bool havesfp = 0;

  sfp.nam = shdrname;
  sfp.dobck = 1;

  static struct bufile lfp;
  ub4 ltabuf = ttend * Cclen * 32U + 0x4000;

  lfp.nam = ltabname;
  lfp.dobck = 1;
  lfp.fd = -1;

  static struct bufile lhfp;
  ub4 lhdrbuf = (kwhshlen + dun0hshlen + dun1hshlen) * 6 + nset * 32U + nstate * 16U + 0x1000U;

  lhfp.nam = lhdrname;
  lhfp.dobck = 1;
  lhfp.fd = -1;

#define Ctulen 64
  char ctuval[Ctulen];
  char nxt[Ctulen];
  char chkbuf[128];
  ub4 chkpos,chklen = 128;

  hitknamlen = max(hitknamlen,hikwlen);
  hitknamlen = max(hitknamlen,7);
  sb4 tknampad = -(hitknamlen+1);

  memset(sposs,0,sizeof(sposs));

  // lexer and parser
  if (pass1) {

    eoflen = specvlens[Sv_tkeof];

    bpos = mysnprintf(buf,0,blen,"\n  ");

    for (tk = 0; tk < nkwd; tk++) {
      len = kwlens[tk];
      nam = kwds[tk];
      bpos += mysnprintf(buf,bpos,blen,"T%*.*s = %2u,%s",tknampad,len,nam,tk,(tk & 7) == 7 ? "\n  " : " ");
      if (tk == nkwd_mrg) bpos += mysnprintf(buf,bpos,blen,"\n  T%*s = %2u, ",tknampad,"99_mrg",tk);
      if (len == eoflen && memeq(nam,speceof,eoflen)) {
        bpos += mysnprintf(buf,bpos,blen,"T%*s = %2u, ",tknampad,"99_eof",tk);
        haveeof=1;
      }
    }
    if (nkwd_mrg == hi16) bpos += mysnprintf(buf,bpos,blen,"\n  T%*s = %2u, ",tknampad,"99_mrg",nkwd);
    bpos += mysnprintf(buf,bpos,blen,"\n  T%*s = %2u,\n\n  ",tknampad,"99_kwd",nkwd);

    vrb("pass 1 %u xtokens:",nltok);
    for (tk = 0; tk < nltok; tk++) {
      len = toklens[tk];
      nam = toks[tk];

      memcpy(spool + spos,nam,len);
      sposs[tk] = sposz;
      spos += len;
      sposz += len + 1;
      spool[spos++] = '\\';
      spool[spos++] = '0';
      bpos += mysnprintf(buf,bpos,blen,"T%*s = %2u,%s",tknampad,fmtname(len,nam),tk+nkwd,(tk & 7) == 7 ? "\n  " : " ");

      grp = 0;
      while (grp <= tkhigrp && tkngrps[grp] != tk) grp++;

      if (grp <= tkhigrp) {
        mysnprintf(buf2,0,32,"%ugrp",grp);
        bpos += mysnprintf(buf,bpos,blen,"\n  T%*s = %2u,\n  ",tknampad,buf2,tk+nkwd);
      }
      if (len == eoflen && memeq(nam,speceof,eoflen)) {
        bpos += mysnprintf(buf,bpos,blen,"T%*s = %2u, ",tknampad,"99_eof",tk+nkwd);
        haveeof=1;
      }
      if (tkpass[tk] == passc) vrbo("%.*s",len,nam);
    }
    if (nltok == 0) {
      mysnprintf(buf2,0,32,"0grp");
      bpos += mysnprintf(buf,bpos,blen,"\n  T%*s = %2u,\n  ",tknampad,buf2,tk+nkwd);
    }

    spool[spos] = 0;
    sposs[tk] = sposz;

    if (haveeof == 0) {
      if (eoflen) sinfo(Lno,"no eof token '%.*s' - using 0",eoflen,speceof);
      bpos += mysnprintf(buf,bpos,blen,"T%*s = %2u, ",tknampad,"99_eof",0);
    }

    mysnprintf(buf,bpos,blen,"\n  T%*s = %u\n",tknampad,"99_count",nltok + nkwd);

    chkpos = mysnprintf(chkbuf,0,chklen," %lx ",chkhsh);
    if (nodiff) rv = 1;
    else {
      rv = filediff(shdrname,chkbuf,chkpos);
    }
   if (rv == -1) return 1;
   else if (rv) {

    havesfp = 1;
    myfopen(FLN,&sfp,shdrbuf,1);

    myfprintf(&sfp,"/* %s - lexer token defines\n\n",shdrname);

    wrfhdr(&sfp,0);

    myfprintf(&sfp,"   signature: @%s@ */\n\n",chkbuf);

    if (nkwd | nltok) {
      myfprintf(&sfp,"enum %sToken {%s};\n\n",packed8,buf);

      myfprintf(&sfp,"#define Kwcnt %u\n",nkwd);
      myfprintf(&sfp,"#define Tknamlen %u\n",hitknamlen);
      myfprintf(&sfp,"#define Tkgrp %u\n\n",tkhigrp + 1);

      myfprintf(&sfp,"static const ub2 toknampos[%u] = {\n  ",nltok+1);
      for (tk = 0; tk <= nltok; tk++) {
        if (tk) myfputc(&sfp,',');
        myfprintf(&sfp,"%u",sposs[tk]);
      }
      myfputs(&sfp,"\n};\n\n");

      myfprintf(&sfp,"static const char toknampool[%u] = \"%s\";\n\n",max(sposz,1),spool);

    if (nkwd < ntkwd) {
      wrenum(&sfp,tkwlens,tkwds,"token",ntkwd,hitknamlen);

      spos = 0;
      for (tk = 0; tk < nkwd; tk++) {
        len = kwlens[tk];
        nam = kwds[tk];
        memcpy(spool + spos,nam,len);
        sposs[tk] = spos;
        spos += len;
      }

      myfprintf(&sfp,"static const ub1  kwnamlens [%2u] = { ",nkwd);
      for (tk = 0; tk < nkwd; tk++) {
        if (tk) myfputc(&sfp,',');
        myfprintf(&sfp,"%-2u",kwlens[tk]);
      }
      myfputs(&sfp," };\n");

      myfprintf(&sfp,"static const ub2  kwnamposs [%2u] = { ",nkwd);
      for (tk = 0; tk < nkwd; tk++) {
        if (tk) myfputc(&sfp,',');
        myfprintf(&sfp,"%-2u",sposs[tk]);
      }
      myfputs(&sfp," };\n");

      myfprintf(&sfp,"static const char kwnampool[%u] = \"",spos);
      myfwrite(&sfp,spool,spos);
      myfputs(&sfp,"\";\n\n");
    } else {
      if (nkwd) {
        myfputs(&sfp,"#define tkwnampool kwnampool\n#define tkwnamposs kwnamposs\n#define tkwnamlens kwnamlens\n");
        myfputs(&sfp,"#define t99_count T99_kwd\n\n");
      }
      myfputs(&sfp,"#define token Token\n#define t99_count T99_count\n\n");
    }

    bpos = mysnprintf(buf,0,blen,"static const ub1 tokhrctl[%u] = { ",nltok);
    for (tk = 0; tk < nltok; tk++) {
      if (tk) buf[bpos++] = ',';
      x1 = tokhrctl[tk];
      if (x1 <= 9) bpos += mysnprintf(buf,bpos,blen,"%u",x1);
      else bpos += mysnprintf(buf,bpos,blen,"0x%x",x1);
    }
    myfprintf(&sfp,"%s };\n\n",buf);

    myfprintf(&sfp,"static char hrtoknams[%u] = \"",max(nltok * 4,1));
    bpos = 0;
    for (tk = 0; tk < nltok; tk++) {
      nam = hrtoks + 4 * tk;
      for (i = 0; i < 4; i++) {
        c = nam[i];
        bpos += mysnprintf(buf,bpos,256,"%s",c ? chprint(c) : "\\0");
      }
    }
    myfprintf(&sfp,"%.*s\"; // %u * 4\n\n",bpos,buf,nltok);

    } // nkwd | nltok

    if (nblt) {
      wrenum(&sfp,bltlens,blts,"Bltin" ,nblt,hibltlen);

      myfprintf(&sfp,"static const ub1 hibltlen = %u;\n",hibltlen);
      myfprintf(&sfp,"static const ub1 mibltlen = %u;\n",mibltlen);
    }

    if (ndun) {
      wrenum(&sfp,dunlens,duns,"Dunder",ndun,hidunlen);
      myfprintf(&sfp,"static const ub1 hidunlen = %u;\n",hidunlen);
    }

    } // file diff or not

    // lexer defs
    myfopen(FLN,&lhfp,lhdrbuf,1);

    myfprintf(&lhfp,"/* %s - lexer definitions\n\n",lhdrname);

    wrfhdr(&lhfp,2);

    myfprintf(&lhfp,"#define Cclen %u\n\n",Cclen);

    // keyword and bltin hashes
    if (nkwd && hikwlen > 2) {
      hc = wrhsh(&lhfp,&sfp,"tkw",tkwds,tkwlens,nkwd < ntkwd ? tkwdmap : nil,ntkwd,kwhsh,kwhshlen,1);

      if (sfp.top) myfprintf(&sfp,"static const ub4 kwnamhsh = 0x%x;\n\n",hc);

      wrhshshift(&lhfp,"Kw",kwdhshshift);
      myfprintf(&lhfp,"static const ub1 hikwlen = %u;\n",hikwlen);
      myfprintf(&lhfp,"static const ub1 mikwlen = %u;\n",mikwlen);

      if (nkwd < ntkwd) {
        myfprintf(&lhfp,"static const ub1 kwhshmap[%u] = { ",ntkwd);
        for (i = 0; i < ntkwd; i++) {
          if (i) myfputc(&lhfp,',');
          myfprintf(&lhfp,"%u",tkwdmap[i]);
        }
        myfputs(&lhfp," }; // token to Token\n\n");

      }
#if 0
      myfprintf(&lhfp,"static const ub1 kwgrps[%u] = { ",nkwd);
        for (i = 0; i < nkwd; i++) {
          if (i) myfputc(&lhfp,',');
          myfprintf(&lhfp,"%u",kwgrps[i]);
        }
        myfputs(&lhfp," };\n\n");
#endif
    }
    if (nblt) {
      myfprintf(&lhfp,"#define Bltcnt %u\n",nblt);
      wrhsh(&lhfp,&sfp,"Bblt",blts,bltlens,nil,nblt,blthsh,blthshlen,1);
      wrhshshift(&lhfp,"Blt",blthshshift);
    }

    if (nkwd | nblt) myfprintf(&lhfp,"#define Hshseed   0x%x\n",hshseed);

    if (ndun) {
      myfprintf(&lhfp,"#define Duncnt %u\n",ndun);
      wrhsh(&lhfp,&sfp,"Ddun0",duns,dunlens,nil,ndun,dun0hsh,dun0hshlen,1);
      wrhsh(&lhfp,&sfp,"Ddun1",duns,dunlens,nil,ndun,dun1hsh,dun1hshlen,0);
      myfprintf(&lhfp,"#define Hshd0seed  0x%x\n",d0hshseed);
      myfprintf(&lhfp,"#define Hshd1seed  0x%x\n\n",d1hshseed);
      wrhshshift(&lhfp,"Dn0",dun0hshshift);
      wrhshshift(&lhfp,"Dn1",dun1hshshift);
    }

    if (havesfp) {
      info("wrote %s",shdrname);
      myfclose(&sfp);
    }

#if 1
    if (nkwd && lokwlen < 3) {
      myfputs(&lhfp,"static inline enum Token lookupkw2(ub1 c,ub1 d)\n{\n");
      ub1 kno = 0;
      for (i = 0; i < nkwd; i++) {
        if (kwlens[i] != 2) continue;
        p = kwds[i];
        myfprintf(&lhfp,"  %sif (c == '%c' && d == '%c') return T%.*s;\n",kno ? "else " : "     ",p[0],p[1],kwlens[i],kwds[i]);
        kno++;
      }
      myfputs(&lhfp,"  else return T99_count;\n");
      myfputs(&lhfp,"}\n\n");
    } else if (nkwd) myfputs(&lhfp,"#define lookupkw2(c,d) T99_count\n\n");
#endif

    if (nkwd && hikwlen > 2) {
      wrhshlut(&lhfp,"kw",kwhshlut);
    }

    if (nblt && hibltlen > 2) {
      wrhshlut(&lhfp,"blt",blthshlut);
    }

#if 1
    if (nblt && lobltlen < 3) {
      myfputs(&lhfp,"static inline enum Bltin lookupblt2(ub1 c,ub1 d)\n{\n");
      ub1 kno = 0;
      for (i = 0; i < nblt; i++) {
        if (bltlens[i] != 2) continue;
        p = blts[i];
        myfprintf(&lhfp,"  %sif (c == '%c' && d == '%c') return B%.*s;\n",kno ? "else " : "     ",p[0],p[1],bltlens[i],blts[i]);
        kno++;
      }
      myfputs(&lhfp,"  else return B99_count;\n");
      myfputs(&lhfp,"}\n\n");
    } else if (nblt) myfputs(&lhfp,"#define lookupblt2(c,d) B99_count\n\n");
#endif

#if 1
    if (ndun && lodunlen < 3) {
      myfputs(&lhfp,"static inline enum Dunder lookupdun2(ub1 c,ub1 d)\n{\n");
      ub1 kno = 0;
      for (i = 0; i < ndun; i++) {
        if (dunlens[i] != 2) continue;
        p = duns[i];
        myfprintf(&lhfp,"  %sif (c == '%c' && d == '%c') return D%.*s;\n",kno ? "else " : "     ",p[0],p[1],dunlens[i],duns[i]);
        kno++;
      }
      myfputs(&lhfp,"  else return D99_count;\n");
      myfputs(&lhfp,"}\n\n");
    } else if (ndun) myfputs(&lhfp,"#define lookupdun2(c,d) D99_count\n\n");
#endif

    printsets(&lhfp,2);

    myfprintf(&lhfp,"#define x  %u\n\nstatic unsigned char ctab[256] = {\n  ",nsets);
    bpos = bpos2 = 0;
    for (c = 0; c < 256; c++) {
      if (c) { buf[bpos++] = ','; buf2[bpos2++] = ','; }
      if (c && (c & 0xf) == 0) {
        bpos += mysnprintf(buf,bpos,blen,"  // %.*s\n  ",bpos2,buf2);
        bpos2 = 0;
      }
      if (ctab[c] == nsets) { buf[bpos++] = ' '; buf[bpos++] = 'x'; buf2[bpos2++] = ' '; buf2[bpos2++] = 'x'; }
      else {
        bit = ctab[c];
        set = setmaps[bit];
        nam = setnams + set * Snam;
        bpos += mysnprintf(buf,bpos,blen,"%2u",set);
        if (nam[2]) {
          bpos2 += mysnprintf(buf2,bpos2,blen2,"%2u",set);
        } else {
         buf2[bpos2++] = nam[0]; buf2[bpos2++] = nam[1];
        }
      }
    }
    myfprintf(&lhfp,"%s\n};\n#undef x\n",buf);

    if (cctab_use) {
      myfprintf(&lhfp,"\nstatic unsigned %s utab[256] = {\n  ",nsetm > 8 ? "short" : "char");
      bpos = 0;
      for (c = 0; c < 256; c++) {
        if (c) buf[bpos++] = ',';
        if (c && (c & 0xf) == 0)  bpos += mysnprintf(buf,bpos,blen,"\n  ");
        bpos += mysnprintf(buf,bpos,blen,"%-3u",cctab[c]);
      }
      myfprintf(&lhfp,"%s\n};\n\n",buf);
    }

    if (omittrans == 0) printtrans(&lhfp);

    myfclose(&lhfp);
    info("wrote %s",lhdrname);
  } // pass 1

  myfopen(FLN,&lfp,ltabuf,1);

  myfprintf(&lfp,"/* %s - lexer core, pass %c\n\n",ltabname,passc);

  wrfhdr(&lfp,1);

  printsets(&lfp,1);

  myfputs(&lfp,"\n");

  act = act_init;
  if (act < nact) {
    myfprintf(&lfp,"\n// ln %u init\n%.*s\n",actlns[act],actvlens[act],actvals + act * Actlen);
  }

  for (st0 = 0; st0 < nstate; st0++) { // each state

    memcpy(st0nam,states[st0],stlens[st0]);
    st0nam[stlens[st0]] = 0;

    for (ttndx = 0; ttndx < transtablen; ttndx++) {
      tp0 = transtab + ttndx;
      lno = tp0->ln;
      if (tp0->st0 == st0) break;
    }
    if (ttndx == transtablen) serror(lno|Lno,"unknown state %s",st0nam);

    for (ttend = ttndx; ttend < transtablen; ttend++) {
      tp = transtab + ttend;
      if (tp->st0 != st0) break;
    }
    lnx = lno|Lno;

    // vrb("st %u tt %u %u",st0,ttndx,ttend);
    if (ttend - ttndx < 2) svrb(lnx,"state %s has only one pattern",st0nam);

    ccnt = tcnt = ucnt = ocnt = 0;
    loopc = 0xff;
    minlen = Cclen;

    dotswitch = 1;

    ctl = 0;
    memset(ctltab,0,sizeof(ctltab));
    for (ttndx2 = ttndx; ttndx2 < ttend; ttndx2++) {
      tp = transtab + ttndx2;
      st = tp->st;
      sp = tp->syms;
      cp = tp->ctls;
      len = tp->cclen;
      lno = tp->ln;
      dobt = st == st0 ? 0 : tp->dobt;
      minlen = min(minlen,len - dobt);
      prvctl = ctl;
      ctl = cp[0];
      s = sp[0];
      switch(ctl) {
      case Cc_a:
      case Cc_z:
      case Cc_r:
      case Cc_q: ccnt++; break;
      case Cc_c: ccnt++; ctltab[s] = lno;
                 if (tp->dowhile) { info("state %s loop on '%s'",st0nam,chprint(s)); loopc = s; }
                 break;

      case Cc_t: if (s >= nsets) err(tp,st,"invalid t set %u/%u",s,nsets);
                 if (len == 1 && !dobt) tcnt++;
                 else ccnt++;
                 break;

      case Cc_u: ucnt++; break;
      case Cc_x: if (ttndx2 != ttend-1) err(tp,st,"pat %u: other needs to be last",transtab[ttndx2].patno);
                 if (dobt) dotswitch = 0;
                 break;
      case Cc_e: if (prvctl == Cc_c) ccnt++; break;
      }
    }
    totcnt = ccnt + tcnt + ucnt;
    if (totcnt < 2) svrb(lnx,"one pattern for %u c entries",ccnt);

    if (minlen == 0) {
      svrb(lnx,"state %s repeats",st0nam);
      dotswitch = 0;
    }

    bpos = 0;

    tktabcnt = 0;
    hitktab = 0;

    if (ccnt + ucnt > 9 && tcnt > 9) {
      for (c = 0; c < 256; c++) {
        lno = ctltab[c];
        if (lno) sinfo(lnx,"ctype '%s'",chprint(c));
      }
      err(tp0,st0,"mixed types: %u c %u t %u u",ccnt,tcnt,ucnt);

    // prepare switch for t
    } else if (dotswitch && tcnt > 3) {

      memset(ctbl,0xff,nsets);
      memset(ttbl,0xff,nsets);
      his = 0;
      for (ttndx2 = ttndx; ttndx2 < ttend; ttndx2++) {
        tp = transtab + ttndx2;
        sp = tp->syms;
        cp = tp->ctls;
        lno = tp->ln;
        len = tp->cclen;
        st = tp->st;
        ctl = cp[0];
        s = sp[0];
        if (ctl == Cc_x) {
          ttany = ttndx2;
          svrb(lnx,"any for %s.%s",st0nam,transtab[ttany].pat);
          continue;
        }
        if (ctl != Cc_t || len > 1 || tp->dobt) continue;
        if (s >= nsets) err(tp0,st0,"pat '%s' has invalid t %u",tp->pat,s);
        if (ctbl[s] < 0xff) err(tp0,st0,"duplicate pattern %s",tp->pat);
        ctbl[s] = ctl;
        ttbl[s] = ttndx2;
        if (s > his) his = s;
      }

      for (s = 0; s < nsets; s++) {
        if (ctbl[s] == 0xff) ttbl[s] = ttany;
      }
      bpos += mysnprintf(buf,bpos,blen,"\n  void *compgo%c_%s[%u] = {\n  ",passc,st0nam,nsets+1);

      tkpos = 0;
      for (t = 0; t < nsets; t++) {
        isany = (ctbl[t] == 0xff);
        if (ttbl[t] == 0xff) continue;
        tp = transtab + ttbl[t];
        st = tp->st;
        tk = tp->tk;
        lno = tp->ln;

        mysnprintf(ctuval,0,Ctulen,"\t// t=%s%cln %u",setnams + t * Snam,ena_lno,lno);

        if (tp->iserr) {
          tp->tswitch = 1;
          sinfo(lnx,"err for %s.%s",st0nam,tp->pat);
          bpos += mysnprintf(buf,bpos,blen,"&&lxer_%s_%u_%c,%s ",st0nam,tp->patno,passc,ctuval);
        } else {
          tp->tswitch = 1;
          if (tp->eof && tp->havecode == 0) bpos += mysnprintf(buf,bpos,blen,"&&lx_eof");
          else bpos += mysnprintf(buf,bpos,blen,"&&lx_%.*s",stlens[st],states[st]); // destination
          if (isany) set = nset;
          else set = setmaps[t];
          if (tp->havecode || (tp->havetok && st != st0)) bpos += mysnprintf(buf,bpos,blen,"_C%s_%u",setnams + set * Snam,st0);
          else if (tp->havetok) {
            bpos += mysnprintf(buf,bpos,blen,"_gentk_%u",st0);
            tktabcnt++;
            if (t > hitktab) hitktab = t;
          }
          bpos += mysnprintf(buf,bpos,blen,",%s %s",ctuval,setnams + set * Snam);
          if (tk < nltok) bpos += mysnprintf(buf,bpos,blen," tk %.*s",toklens[tk],toks[tk]);
        }
        bpos += mysnprintf(buf,bpos,blen,"\n  ");
      } // each t

      // other for t
      tp = transtab + ttany;
      if (tp->iserr) mysnprintf(ctuval,0,Ctulen,"lxer_%s_%u_%c",st0nam,tp->patno,passc);
      else {
        st = tp->st;
        mysnprintf(ctuval,0,Ctulen,"lx_%.*s",stlens[st],states[st]);
      }
      myfprintf(&lfp,"%.*s\n  &&%s};\t// ln %u\n\n",bpos,buf,ctuval,tp->ln);

      for (t = 0; t <= hitktab; t++) {
        if (tkpos) {
          tkbuf[tkpos++] = ',';
        }
        nam = "99_count";
        nlen = 8;
        grp = 0;
        if (ctbl[t] != 0xff) {
          tp = transtab + ttbl[t];
          tk = tp->tk;
          if (tp->havetok) {
            nlen = toklens[tk];
            nam = toks[tk];
          }
        }
        tkpos += mysnprintf(tkbuf,tkpos,tklen,"T%.*s",nlen,nam);
      }

    } else { // prep t switch
      dotswitch = 0;
    }

    if (tktabcnt) {
      myfprintf(&lfp,"\n  ub1 tktab_%s[%u] = { %.*s };\n\n ",st0nam,hitktab+1,tkpos,tkbuf);
    }

    // state
    bpos = mysnprintf(buf,0,blen,"\n// %.*s ",stdesclens[st0],stdescs[st0]);
    if (ena_lno) bpos += mysnprintf(buf,0,blen,"ln %u ",lno);
    if (tp0->cmt && tp0->cmtlen) { memcpy(buf+bpos,tp0->cmt,tp0->cmtlen); bpos += tp0->cmtlen; }
    bpos += mysnprintf(buf,bpos,blen," c %u  t %u  u %u\n",ccnt,tcnt,ucnt);

    if (st0 == 0 && nltok) { // gen token
      bpos += mysnprintf(buf,bpos,blen,"lx_%s_tk:\n",st0nam);
      act = act_token;
      if (act < nact) bpos += mysnprintf(buf,bpos,blen,"  %.*s\n",actvlens[act],actvals + act * Actlen);
    }
    bpos += mysnprintf(buf,bpos,blen,"lx_%s:\n  ",st0nam);

    myfprintf(&lfp,"%.*s\n  ",bpos,buf);
    bpos = 0;

    if (loopc != 0xff) bpos += mysnprintf(buf,bpos,blen,"while (sp[n] == '%s') n++; // %u \n  ",chprint(loopc),loopc);
    if (totcnt) {
      if (tp0->setN) bpos += mysnprintf(buf,bpos,blen,"N = n; ");
      bpos += mysnprintf(buf,bpos,blen,"c = sp[n%s];",minlen ? "++" : "");
      if (ena_lno) bpos += mysnprintf(buf,bpos,blen," // ln %u",lno);
      buf[bpos++] = '\n';
    }
    myfwrite(&lfp,buf,bpos);
    bpos = 0;

    ifcnt = 0;
    haveany = 0;
    havetk = 0;

    havetvar = haveuvar = 0;

    // c and t+ as if-else
    for (ttndx2 = ttndx; ttndx2 < ttend; ttndx2++) {
      tp = transtab + ttndx2;
      sp = tp->syms;
      cp = tp->ctls;
      len = tp->cclen;
      st = tp->st;
      tk = tp->tk;
      lno = tp->ln;
      dobt = tp->dobt;

      ctl = cp[0];
      s = sp[0];
      if (tp->tswitch) continue;
      else if (tp->dowhile) continue;
      isany = 0;

      snam=nil;
      switch(ctl) {
      case Cc_t:
        if (tcnt == 1 && setcnts[s] == 1) {
          ctl = Cc_c;
          s = setval0s[s];
        }
        Fallthrough
      case Cc_u:
        snam = strupper(setnams + s * Snam); break;
      default: break;
      }

      rcnt = 0;
      switch(ctl) {
      case Cc_z:
      case Cc_c: mysnprintf(ctuval,0,Ctulen,"c == %s",printcchr(s)); break;
      case Cc_a: mysnprintf(ctuval,0,Ctulen,"(c | 0x20) == '%s'",chprint(s)); break;
      case Cc_q: mysnprintf(ctuval,0,Ctulen,"c == Q"); break;
      case Cc_r: mysnprintf(ctuval,0,Ctulen,"R%u == %u",(s >> 4),s & 0xf); rcnt = 1; break;
      case Cc_t: if (havetvar) mysnprintf(ctuval,0,Ctulen,"t == %s",snam);
                 else if (tcnt > 1) { mysnprintf(ctuval,0,Ctulen," (t = ctab[c]) == %s",snam); havetvar=1; }
                 else mysnprintf(ctuval,0,Ctulen,"ctab[c] == %s",snam);
                 break;
      case Cc_u: if (haveuvar) mysnprintf(ctuval,0,Ctulen," (u & %s) ",snam);
                 else if (ucnt > 1) mysnprintf(ctuval,0,Ctulen," ( (u = utab[c]) & %s) ",snam);
                 else mysnprintf(ctuval,0,Ctulen," (utab[c] & %s) ",snam);
                 haveuvar=1;
                 break;
      case Cc_x: isany = 1; break;
      case Cc_e: break;
      }

      if (!isany) {
        bpos += mysnprintf(buf,bpos,blen,"  %sif (%-10s",ifcnt ? "else " : "    ",ctuval);
        ifcnt++;
      }

      for (i = 1; i < len; i++) {
        i2 = i - minlen;
        i2 -= min(i2,rcnt);
        if (isany) err(tp0,st0,"other at index %u",i);
        prvctl = ctl;
        ctl = cp[i];
        s = sp[i];
        if (i2) mysnprintf(nxt,0,Ctulen,"sp[n+%u]",i2);
        else mysnprintf(nxt,0,Ctulen,"sp[n]");

        switch(ctl) {
        case Cc_z:
        case Cc_c: mysnprintf(ctuval,0,Ctulen," %s == %s",nxt,printcchr(s)); break;
        case Cc_a: mysnprintf(ctuval,0,Ctulen,"(%s | 0x20) == '%s'",nxt,chprint(s)); break;
        case Cc_q: mysnprintf(ctuval,0,Ctulen," %s == Q",nxt); break;
        case Cc_r: mysnprintf(ctuval,0,Ctulen,"R%u == %u",(s >> 4),s & 0xf); rcnt++; break;
        case Cc_t: mysnprintf(ctuval,0,Ctulen," ctab[%s] == %s",nxt,strupper(setnams + s * Snam)); break;
        case Cc_u: mysnprintf(ctuval,0,Ctulen," (utab[%s] & %s)",nxt,strupper(setnams + s * Snam)); break;
        case Cc_x: err(tp0,st0,"unexpected other at pos %u",i);
        case Cc_e: if (i2 == 0) err(tp0,st0,"equal at zero inc, pat %s",tp->pat);
                   if (prvctl == Cc_t && i == 1 && havetvar) mysnprintf(ctuval,0,Ctulen," ctab[%s] == t",nxt);
                   else if (prvctl == Cc_t && i == 1) mysnprintf(ctuval,0,Ctulen," ctab[%s] == ctab[c]]",nxt);
                   else if (prvctl == Cc_t) mysnprintf(ctuval,0,Ctulen," ctab[sp[n+%u]] == ctab[sp[n+%u]]",i2,i2-1);
                   else if (prvctl == Cc_c && i == 1) mysnprintf(ctuval,0,Ctulen," %s == c",nxt);
                   else if (prvctl == Cc_c) mysnprintf(ctuval,0,Ctulen," sp[n+%u] == sp[n+%u]",i2,i2-1);
                   else err(tp0,st0,"unsupported EQ type %u",prvctl);
                   break;
        }
        bpos += mysnprintf(buf,bpos,blen," && %-10s",ctuval);
      } // each lookahead

      myfwrite(&lfp,buf,bpos);
      bpos = 0;

      s = tp->notpred;
      if (s) bpos += mysnprintf(buf,bpos,blen," && sp[n-1] != '%s'",chprint(s));

      if (isany && dotswitch) {
        // bpos += mysnprintf(buf,bpos,blen,"; }");
        if (ena_lno) bpos += mysnprintf(buf,bpos,blen," // ln %u csw any",lno);
        buf[bpos++] = '\n';
        myfwrite(&lfp,buf,bpos);
        bpos = 0;
        continue;
      } else if (isany) bpos += mysnprintf(buf,bpos,blen," %s { ",ifcnt ? "else " : "   ");
      else bpos += mysnprintf(buf,bpos,blen,") { ");

      myfwrite(&lfp,buf,bpos);
      bpos = 0;

      codlen = tp->codlen;

      if (dobt || tp->eof) len = 0;
      inclen = len;
      if (minlen && inclen) inclen--;
      inclen -= min(inclen,rcnt);
      if (inclen == 1) bpos += mysnprintf(buf,bpos,blen,"n++; ");
      else if (inclen) bpos += mysnprintf(buf,bpos,blen,"n += %u; ",inclen);

      if (codlen && omitcode == 0) {
        if (codlen > 48) bpos += mysnprintf(buf,bpos,blen,"\n  ");
        bpos = wrcode(buf,bpos,blen,codlen,tp->code,lno);
        buf[bpos++] = '\n';
      }

      if (tk < nltok) {
        if (omittoken == 0) {
          havetk = 1;
          bpos += mysnprintf(buf,bpos,blen,"tk = T%.*s; ",toklens[tk],toks[tk]);
        }
      }

      // goto part
      nst = states[st];
      if (tp->iserr) bpos += mysnprintf(buf,bpos,blen,"goto lxer_%s_%u_%c;",st0nam,tp->patno,passc);
      else if (tp->eof) bpos += mysnprintf(buf,bpos,blen,"goto lx_eof;");
      else if (tp->actstate == 0) {
        bpos += mysnprintf(buf,bpos,blen,"goto lx_%.*s%s;",stlens[st],nst,havetk ? "_tk" : "");
      }
      bpos += mysnprintf(buf,bpos,blen," // csw");
      if (ena_lno) bpos += mysnprintf(buf,bpos,blen," ln %u",lno);
      buf[bpos++] = '\n';
      buf[bpos++] = '}';
      havetk = 0;

      myfwrite(&lfp,buf,bpos);
      bpos = 0;

    } // each pat for if-else

    if (dotswitch == 0 ) {
      myfprintf(&lfp,"  %.*s\n",bpos,buf);
      continue;
    }

    if (ifcnt) bpos += mysnprintf(buf,bpos,blen,"  else { %s %s goto *compgo%c_%.*s[t];}",havetvar ? "" : "t = ctab[c];",minlen ? "" : "n++;",passc,stlens[st0],states[st0]);
    else bpos += mysnprintf(buf,bpos,blen," %s %s goto *compgo%c_%.*s[t];",havetvar ? "" : "t = ctab[c];",minlen ? "" : "n++;",passc,stlens[st0],states[st0]);

    myfprintf(&lfp,"  %.*s\n",bpos,buf);
    bpos = 0;

    // write code parts for t switch
    tktabcnt = 0;
    for (ttndx2 = ttndx; ttndx2 < ttend; ttndx2++) {
      tp = transtab + ttndx2;
      sp = tp->syms;
      cp = tp->ctls;
      st = tp->st;
      tk = tp->tk;
      lno = tp->ln;

      ctl = cp[0];
      s = sp[0];

      if (tp->havecode == 0) {
        if (tp->havetok == 0) continue;
        else if (st == st0) { tktabcnt++; continue; }
      } else if (tp->tswitch == 0) continue;

      bpos += mysnprintf(buf,bpos,blen,"\nlx_%.*s",stlens[st],states[st]);
      if (tp->tswitch) {
        if (tp->havecode || st != st0) {
          if (tp->ctls[0] == Cc_x) s = nset;
          bpos += mysnprintf(buf,bpos,blen,"_C%s_%u:",setnams + s * Snam,st0);
        }
      } else bpos += mysnprintf(buf,bpos,blen,"_%u_%u:",st0,lno);

      bpos += mysnprintf(buf,bpos,blen," // from %s.%s set %u ctl %u",st0nam,tp->pat,s,ctl);
      if (ena_lno) bpos += mysnprintf(buf,bpos,blen," ln %u",lno);
      buf[bpos++] = '\n';

      if (tp->codlen) {
        bpos = wrcode(buf,bpos,blen,tp->codlen,tp->code,lno);
      }
      if (tk < nltok) {
        bpos += mysnprintf(buf,bpos,blen,"\n  tk = T%.*s;\n",toklens[tk],toks[tk]);
        havetk = 1;
      }
      if (tp->eof) bpos += mysnprintf(buf,bpos,blen,"  goto lx_eof;");
      else if (tp->actstate == 0) bpos += mysnprintf(buf,bpos,blen,"  goto lx_%.*s;",stlens[st],states[st]);

      if (ena_lno) bpos += mysnprintf(buf,bpos,blen," // ln %u tsw",lno);
      buf[bpos++] = '\n';
    } // each pat in switch

    if (tktabcnt) { // todo multiple tswitches
      bpos += mysnprintf(buf,bpos,blen,"\nlx_%s_gentk_%u:\n  ",st0nam,st0);
      bpos += mysnprintf(buf,bpos,blen,"tk = tktab_%s[t]; ",st0nam);
      bpos += mysnprintf(buf,bpos,blen,"goto lx_root_tk;\n");
    }

    myfprintf(&lfp,"  %.*s\n",bpos,buf);
  } // each state

  printsets(&lfp,0);

  for (ttndx = 0; ttndx < ttend; ttndx++) {
    tp = transtab + ttndx;
    st0 = tp->st0;
    if (tp->iserr) myfprintf(&lfp,"lxer_%.*s_%u_%c: lxerror(l,n-nlcol,\"%.*s\",nil,c,\"unexpected char\");\n",
      stlens[st0],states[st0],tp->patno,passc,stlens[st0],states[st0]);
  }

  myfprintf(&lfp,"\nlx_eof:\n\n");

  myfclose(&lfp);
  info("wrote %s",ltabname);

  return 0;
}

enum Cmdopt { Co_pass1=1,Co_pass2,Co_omit,Co_nowrite,Co_nodif,Co_until,Co_Winfo };

static struct cmdopt cmdopts[] = {
  { "",'1',      Co_pass1,  nil,        "generate pass 1" },
  { "",'2',      Co_pass2,  nil,        "generate pass 2" },
  { "",'n',      Co_nowrite,nil,        "do not write" },
  { "",'u',      Co_nodif,  nil,        "unconditional write" },
  { "omit", ' ', Co_omit,   "%ecode,trans,token", "omit section" },
  { "until", ' ',Co_until,  "%espec,gen,out", "process until <pass>" },
  { "Winfo",' ', Co_Winfo,  "list",     "comma-separated list of diags to report as info" },
  { nil,0,0,"<spec> <synhdr> <lextab>", "genlex"}
};

static int cmdline(int argc, char *argv[])
{
  ub4 orgargc = (ub4)argc;
  struct cmdval coval;
  struct cmdopt *op;
  enum Parsearg pa;
  bool havereg,endopt;

  globs.msglvl = Info;
  globs.rununtil = 0xff;

  while (argc) { // options
    havereg = 0;
    endopt = 0;
    pa = parseargs(argc,argv,cmdopts,&coval,nil,1);

    switch (pa) {
    case Pa_nil:
    case Pa_eof:
    case Pa_min2:   endopt = 1; break;

    case Pa_min1:
    case Pa_plusmin:
    case Pa_plus1:
    case Pa_regarg: havereg = 1; break; // first non-option regular

    case Pa_notfound:
      error("option '%.*s' at arg %u not found",coval.olen,*argv,orgargc - argc);
      return 1;

    case Pa_noarg:
      error("option '%.*s' missing arg",coval.olen,*argv);
      return 1;

    case Pa_found:
    case Pa_genfound:
      if (coval.err) { error("option '%.*s' invalid arg %u",coval.olen,*argv,coval.err); return 1; }
      argc--; argv++;
      break;
    case Pa_found2:
    case Pa_genfound2:
      if (coval.err) { error("option '%.*s' invalid arg %u",coval.olen,*argv,coval.err); return 1; }
      argc -=2; argv += 2;
      break;
    }

    if (havereg) break;
    else if (endopt) { argc--; argv++; break; }

    op = coval.op;
    switch(op->opt) {
    case Co_help:    return 1;
    case Co_version: return 1;
    case Co_license: return 1;

    case Co_omit:  switch (coval.uval) {
                   case 0: omitcode = 1; break;
                   case 1: omittrans = 1; break;
                   case 2: omittoken = 1; break;
                   }
                   break;
    case Co_until: globs.rununtil = coval.uval; break;
    case Co_pass1: pass1 = 1; pass = 1; passc = '1'; break;
    case Co_pass2: pass2 = 1; pass = 2; passc = '2'; break;
    case Co_nowrite: nowrite = 1; break;
    case Co_nodif: nodiff = 1; break;
//    case Co_Winfo: diaset(Info,(cchar *)coval.sval); break;
    }
    if (coval.err) return 1;
  }
  if ((pass1 | pass2) == 0) {
    pass1 = 1; pass = 1; passc = '1';
  }

  while (argc) { // regular args
    if (!specname) specname = (cchar *)*argv;
    else if (!ltabname) ltabname = (cchar *)*argv;
    else if (!lhdrname) lhdrname = (cchar *)*argv;
    else if (!shdrname) shdrname = (cchar *)*argv;
    else warning("ignoring extra arg '%s'",*argv);
    argc--; argv++;
  }

  setmsglvl(globs.msglvl,msgopts);
  return 0;
}

static void init(void)
{
  inios();
  globs.maxvm = 4;
  oslimits();
  setsigs();

  inimem();
  inimsg(msgopts);
  inimath();
}

void bugreport(cchar *rep,ub2 rlen,cchar *tag) {}

static int do_main(int argc, char *argv[])
{
  int rv = 1;

  ub8 T0=0;

  timeit(&T0,nil);

  init();

  if (argc > 0) {
    prgnam = strrchr(argv[0],'/');
    if (prgnam) prgnam++; else prgnam = argv[0];
    argc--; argv++;
  } else prgnam = "genlex";

  globs.prgnam = prgnam;

  iniutil();
  iniprgtim();

  rv = cmdline(argc,argv);
  if (rv) return 1;

  else if (!specname) { error("%s: missing input spec file",prgnam); return 1; }
  else if (!ltabname) { error("%s: missing output lex tab file",prgnam); return 1; }
  else if (pass1 && !lhdrname) { error("%s: missing output lex hdr file",prgnam); return 1; }
  else if (pass1 && !shdrname) { error("%s: missing output syn hdr file",prgnam); return 1; }

  sassert(sizeof(ctab) == 256,"short enums required");

  if (rdspec(specname)) return 1;
  if (globs.rununtil < 1) { info("until %u", globs.rununtil); return 0; }

  if (mkhshes()) return 1;
  if (mktables()) return 1;
  if (globs.rununtil < 2) { info("until %u", globs.rununtil); return 0; }

  if (wrfile()) return 1;

  timeit(&T0,"lexer generation took");

  return 0;
}

int main(int argc, char *argv[])
{
  int rv;

  rv = do_main(argc,argv);

  eximsg();
  eximem(rv == 0);
  exios(rv == 0);

  return rv || globs.retval;
}
