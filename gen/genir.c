/* genir.c - generate ir defines

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

#include <stdarg.h>
#include <string.h>

#include "../../base.h"

#include "../fmt.h"

#include "../mem.h"
#include "../os.h"

static ub4 msgfile = Shsrc_genir;
#include "../msg.h"

#include "../util.h"

#include "../tim.h"

#include "../irtyp.h"

#if defined __clang__
// #pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined __GNUC__
// #pragma GCC diagnostic ignored "-Wsign-conversion"
// #pragma GCC diagnostic ignored "-Wpointer-sign"
#endif

static const char version[] = "0.1.0-alpha";

static const char *irdefname; // arg 1

struct globs globs;

static ub2 msgopts = Msg_shcoord | Msg_fno | Msg_lno | Msg_col;

enum Field { Insgrp,Typ,Regd,
  Ldins,Ldmod,Ldimm,Ldoir,Ldshl,Ldtyp2,Ldoir2,
//  Stins,Stmod,Stimm,Stoir,Stshl,Sttyp2,Stoir2,
  Aop,Amod,Aoir,
  Opsiz,Ctins,Cc,Ccofs,Regs1,Regs2,
  Ctmod,Ctoir,
  Fldcnt };

static ub4 cnts[Fldcnt];
static ub2 bits[Fldcnt];
static ub2 mbits[Fldcnt];
static ub2 lbits[Fldcnt];
static ub4 msks[Fldcnt];
static cchar *nams[Fldcnt] = {
  "insgrp","typ","regd",
  "ldins","ldmod","Ldimm","Ldoir","Ldshl","Ldtyp2","Ldoir2",
//  "stins","stmod","stimm","stoir","stshl","sttyp2","stoir2",
  "aop","amod","aoir",
  "opsiz","ctins","cc","ccofs","regs1","regs2",
  "ctmod","ctoir" };

static int mkdefs(void)
{
  ub2 f;
  ub2 bit,mbit,lbit,insbit,rbit,sbit;
  ub4 cnt,Cnt,res,msk;
  cchar *nam;

  cnts[Opsiz] = Sizcnt;
  cnts[Insgrp] = Ig_cnt;
  cnts[Typ] = Ty_cnt;

  cnts[Ldins] = Ld_cnt;
  cnts[Regd] = Iregcnt;
  cnts[Ldmod] = Modcnt;
  cnts[Ldshl] = 1U << Ildshlbits;
  cnts[Ldtyp2] = Ty_cnt;

/*
  cnts[Stins] = Ld_cnt;
  cnts[Stmod] = Modcnt;
  cnts[Stshl] = 1U << Ildshlbits;
  cnts[Sttyp2] = Ty_cnt;
 */

  cnts[Aop] = Opcnt;
  cnts[Regs1] = Iregcnt;
  cnts[Regs2] = Iregcnt;
  cnts[Amod] = Modcnt;

  cnts[Ctins] = Ctcnt;
  cnts[Cc] = Cc_cnt;
  cnts[Ctmod] = Modcnt;

//  infofln(FLN,"spare bits");
  for (f = 0; f < Fldcnt; f++) {
    nam = nams[f];
    cnt = cnts[f];
    if (cnt == 0) continue;
    else if (cnt >= 0x80000000) warning("%s invalid count %u",nam,cnt);
    bit = bits[f] = nxbit(cnt);
    Cnt = (1U << bit);
    info("%-7s %2u %2u",nam,cnt,Cnt);
    res = Cnt - cnt;
    showcnt(nams[f],res);
    msks[f] = Cnt - 1;
  }

// ld/st/ar ins, typ, regd
  mbit = 31; f = Insgrp;
  mbits[f] = mbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;
  insbit = mbit;

  f = Typ;
  mbits[f] = insbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;

  f = Regd;
  mbits[f] = mbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;
  rbit = mbit;

// ld
  f = Ldins;
  mbits[f] = mbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;
  sbit = mbit;

  f = Ldimm;
  mbits[f] = bits[f] = mbit;
  lbits[f] = 0;
  cnt = 1U << mbit;
  cnts[f] = cnt;
  msks[f] = cnt - 1;

  f = Ldmod;
  mbits[f] = sbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;
  sbit = mbit;

  f = Ldoir;
  mbits[f] = bits[f] = mbit;
  lbits[f] = 0;
  cnts[f] = cnt = 1U << mbit;
  msks[f] = cnt - 1;

  f = Ldshl;
  mbits[f] = mbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;

  f = Ldtyp2;
  mbits[f] = sbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;

  f = Ldoir2;
  mbits[f] = mbit;
  lbits[f] = 0;
  cnts[f] = cnt = 1U << mbit;
  msks[f] = cnt - 1;

// st
//  mbits[Arins] = insbit;

// ari
  mbit = rbit;

  f = Regs1;
  mbits[f] = mbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;
  rbit = mbit;

  f = Aop;
  mbits[f] = mbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;

  f = Regs2;
  mbits[f] = mbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;

  f = Amod;
  mbits[f] = mbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;

  f = Aoir;
  mbits[f] = bits[f] = mbit;
  lbits[f] = 0;
  cnts[f] = cnt = 1U << mbit;
  msks[f] = cnt - 1;

// ct
  mbit = rbit;

  f = Ctins;
  mbits[f] = mbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;

  f = Cc;
  mbits[f] = mbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;

  f = Ccofs;
  mbits[f] = bits[f] = mbit;
  lbits[f] = 0;
  cnts[f] = cnt = 1U << mbit;
  msks[f] = cnt - 1;

  mbit = insbit;
  f = Ctmod;
  mbits[f] = mbit;
  mbit -= bits[f];
  lbits[f] = mbit+1;

  f = Ctoir;
  mbits[f] = bits[f] = mbit;
  lbits[f] = 0;
  cnts[f] = cnt = 1U << mbit;
  msks[f] = cnt - 1;

  for (f = 0; f < Fldcnt; f++) {
    nam = nams[f];
    cnt = cnts[f];
    msk = msks[f];
    mbit = mbits[f];
    lbit = lbits[f];
    if (cnt == 0 || cnt >= 0x80000000) warning("%s invalid count %u",nam,cnt);
    if (msk == 0 || msk == hi32) warning("%s invalid mask %x",nam,msk);
    if (mbit == 0 || mbit > 31) warning("%s invalid hi bit %u",nam,mbit);
    if (lbit >= mbit) warning("%s invalid lo bit %u for hi %u",nam,lbit,mbit);
  }

  return 0;
}

static void wrfhdr(struct bufile *fp)
{
  ub8 mtim = osfiltim("irdef.h");
  ub4 dmin = (ub4)(mtim / 60);
  ub4 dtim = nixday2cal(dmin / (24 * 60));

  myfprintf(fp,"   generated by genir %s %s\n\n",version,fmtdate(globs.prgdtim,globs.prgdmin));

  myfprintf(fp,"   from irtyp.h %s */\n\n",fmtdate(dtim,dmin));
}

static void wrfld(struct bufile *fp,enum Field f)
{
  cchar *nam = nams[f];

  ub4 msk = msks[f];

  myfprintf(fp,"#define %sbit %u // %u\n#define %smsk ",nam,lbits[f],cnts[f],nam);
  if (msk <= 9) myfprintf(fp,"%u\n\n",msk);
  else myfprintf(fp,"0x%x\n\n",msk);
}

static ub2 addln(char *buf,ub2 pos,ub2 len,enum Field f,ub2 ind)
{
  pos += mysnprintf(buf,pos,len,"%2u-%2u %s",mbits[f],lbits[f],nams[f]);
  if (ind) pos += mysnprintf(buf,pos,len,"\n%*s",ind," ");
  else { buf[pos++] = ' '; buf[pos++] = ' '; }
  return pos;
}

static int wrfile(void)
{
  ub2 pos;
  ub2 blen = 1024;
  char buf[1024];

  static struct bufile sfp;
  ub4 bulen = 4096;

  enum Field f;

  sfp.nam = irdefname;
  sfp.dobck = 2;

  myfopen(&sfp,bulen,1);

  myfprintf(&sfp,"/* %s - ir defines\n\n",irdefname);

  wrfhdr(&sfp);

  for (f = 0; f < Fldcnt; f++) {
    wrfld(&sfp,f);
  }

  myfputs(&sfp,"\n/*\n");

// ld
  pos = mysnprintf(buf,0,blen,"\nld ");
  pos = addln(buf,pos,blen,Insgrp,0);
  pos = addln(buf,pos,blen,Typ,0);
  pos = addln(buf,pos,blen,Regd,0);

  pos = addln(buf,pos,blen,Ldins,0);

  pos = addln(buf,pos,blen,Ldimm,1);

  pos = addln(buf,pos,blen,Ldmod,0);

  pos = addln(buf,pos,blen,Ldoir,2);

  pos = addln(buf,pos,blen,Ldshl,0);

  pos = addln(buf,pos,blen,Ldoir2,2);

  pos = addln(buf,pos,blen,Ldtyp2,0);

  buf[pos++] = '\n';

  myfwrite(&sfp,buf,pos);

// ari
  pos = mysnprintf(buf,0,blen,"\nar ");
  pos = addln(buf,pos,blen,Insgrp,0);
  pos = addln(buf,pos,blen,Typ,0);
  pos = addln(buf,pos,blen,Regd,0);

  pos = addln(buf,pos,blen,Regs1,0);
  pos = addln(buf,pos,blen,Regs2,0);
  pos = addln(buf,pos,blen,Aop,0);
  pos = addln(buf,pos,blen,Amod,0);
  pos = addln(buf,pos,blen,Aoir,1);

  buf[pos++] = '\n';

  myfwrite(&sfp,buf,pos);

// cc
  pos = mysnprintf(buf,0,blen,"\nct ");
  pos = addln(buf,pos,blen,Insgrp,0);
  pos = addln(buf,pos,blen,Typ,0);
  pos = addln(buf,pos,blen,Regd,0);
  pos = addln(buf,pos,blen,Regs1,0);

  pos = addln(buf,pos,blen,Ctins,0);
  pos = addln(buf,pos,blen,Cc,0);
  pos = addln(buf,pos,blen,Ccofs,1);

  pos = addln(buf,pos,blen,Ctmod,0);
  pos = addln(buf,pos,blen,Ctoir,1);

  buf[pos++] = '\n';

  myfwrite(&sfp,buf,pos);

  myfprintf(fp,"\n\n  generated by genir %s %s\n */\n",version,fmtdate(globs.prgdtim,globs.prgdmin));

  return myfclose(&sfp);
}

enum Cmdopt { Co_until=1,Co_uncond,Co_Winfo };

static struct cmdopt cmdopts[] = {
//  { "trace",'t',Co_trace,"?%ulevel","enable tracing" },
//  { "Winfo",' ',Co_Winfo,"list","comma-separated list of diags to report as info" },
  { "",'u',      Co_uncond, "",         "unconditional write" },
  { "until", ' ',Co_until, "%espec,gen,out", "process until <pass>" },
  { nil,3,0,"<spec> <syntab> <syndef>","genir"}
};

static int cmdline(int argc, char *argv[])
{
  ub4 orgargc = (ub4)argc;
  struct cmdval coval;
  struct cmdopt *op;
  enum Parsearg pa;
  cchar *prgnam;
  bool havereg,endopt;

  if (argc > 0) {
    prgnam = strrchr(argv[0],'/');
    if (prgnam) prgnam++; else prgnam = argv[0];
    argc--; argv++;
  } else prgnam = "genir";

  globs.prgnam = prgnam;
  globs.msglvl = Info;
  globs.rununtil = 0xff;

  iniutil();
  iniprgtim();

  while (argc) { // options
    havereg = 0;
    endopt = 0;
    pa = parseargs((ub4)argc,argv,cmdopts,&coval,nil,1);

    switch (pa) {
    case Pa_nil:
    case Pa_eof: endopt = 1; break;

    case Pa_min2: endopt = 1; break;

    case Pa_min1:
    case Pa_plusmin:
    case Pa_plus1: havereg = 1; break;

    case Pa_regarg: havereg = 1; break; // first non-option regular

    case Pa_notfound:
      error("option '%.*s' at arg %u not found",coval.olen,*argv,orgargc - (ub4)argc);
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
    case Co_help: return 1;
    case Co_version: return 1;
    case Co_license: return 1;
    case Co_until: globs.rununtil = coval.uval; break;
//    case Co_Winfo: diaset(Info,(cchar *)coval.sval); break;
//    case Co_trace: trclvl = coval.uval; diaset(Vrb,(cchar *)coval.sval); break;
    }
    if (coval.err) return 1;
  }

  while (argc) { // regular args
    if (!irdefname) irdefname = (cchar *)*argv;
    else warning("ignoring extra arg '%s'",*argv);
    argc--; argv++;
  }

  setmsglvl(globs.msglvl,msgopts);
  return 0;
}

static void init(void)
{
  setsigs();

  inios();
  inimem();
  inimsg(msgopts);
}

static int do_main(int argc, char *argv[])
{
  int rv = 1;

  rv = cmdline(argc,argv);
  if (rv) return 1;
  else if (!irdefname) { errorfln(FLN,0,"missing ir def file"); return 1; }

  sassert(sizeof(ub8) == 8,"expect long long to be 8 bytes");

  if (mkdefs()) return 2;

  if (wrfile()) return 2;
  info("wrote %s",irdefname);

  return 0;
}

int main(int argc, char *argv[])
{
  int rv;

  init();

  rv = do_main(argc,argv);

  if (rv > 1) warning("%s exiting with error",globs.prgnam);
  eximsg();
  eximem(rv == 0);
  exios(rv == 0);

  return rv || globs.retval;
}
