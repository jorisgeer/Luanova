/* vmrun.c -

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

#ifdef Vmrun

#include <stdarg.h>
//#include <stddef.h>
#include <string.h>

#include "base.h"

#include "fmt.h"

#include "mem.h"
#include "os.h"

static ub4 msgfile = Shsrc_vmrun;
#include "msg.h"

#include "util.h"

#include "tim.h"

#include "irtyp.h"
#include "irdef.h"

extern void runir(const ub4 *prg,ub4 *mem);

#if defined __clang__
// #pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined __GNUC__
// #pragma GCC diagnostic ignored "-Wsign-conversion"
// #pragma GCC diagnostic ignored "-Wpointer-sign"
#endif

static const char version[] = "0.1.0-alpha";

static const char *bcfname; // arg 1

struct globs globs;

static ub2 msgopts = Msg_shcoord | Msg_fno | Msg_lno | Msg_col;

/*
first_num = 2
second_num = 8
num_of_terms = ...

while(num_of_terms):
   third_num = first_num + second_num
   first_num=second_num
   second_num=third_num
   print(third_num)
   num_of_terms=num_of_terms-1
10
18
28
46
74
120

def tst(n):
  a = 0

  while (n > 0):
    a = a + 2;
    n = n - 1

  print(a)


tst(1 << 24)  # 33554432

*/

enum varnam { Nvar, Avar };

#define In(x) ((x) << insgrpbit)

static ub4 prog[64] = {

// head

  // 0 ld reg0, nvar
  (Ig_ld << insgrpbit) | (Ty_u4 << typbit) | (Ld_bas << ldinsbit) | Nvar,

  // 1 bz reg0,tail
  (Ig_ctl << insgrpbit) | (Ty_u4 << typbit) | (Ctbcc << ctinsbit) | (Cz << ccbit) | 9,

  // 2 ld reg0, avar
  (Ig_ld << insgrpbit) | (Ty_u4 << typbit) | (Ld_bas << ldinsbit) | Avar,

  // 3 add reg0,reg0, imm 2
  (Ig_ari << insgrpbit) | (Ty_u4 << typbit) | (Mimm << amodbit) | (Oadd << aopbit) | 2,

  // 4 st reg0, avar
  (Ig_st << insgrpbit) | (Ty_u4 << typbit) | (Ld_bas << ldinsbit) | Avar,

  // 5 ld reg0, nvar
  (Ig_ld << insgrpbit) | (Ty_u4 << typbit) | (Ld_bas << ldinsbit) | Nvar,

  // 6 sub reg0,reg0, imm 1
  (Ig_ari << insgrpbit) | (Ty_u4 << typbit) | (Mimm << amodbit) | (Osub << aopbit) | 1,

  // 7 st reg0, nvar
  (Ig_st << insgrpbit) | (Ty_u4 << typbit) | (Ld_bas << ldinsbit) | Nvar,

  // 8 jmp head
  (Ig_ctl << insgrpbit) | (Ctjmp << ctinsbit) | 0,

// 9 tail: eof
  (Ig_ctl << insgrpbit) | (Ctcal << ctinsbit) | ccofsmsk,
  (Ig_ctl << insgrpbit) | (Ctcal << ctinsbit) | ccofsmsk,
  hi32,
  hi32,
  hi32
};

static ub4 lvarmem[64];
static volatile ub4 vx;

static void dotest(void)
{
  ub4 n = 1 << 24;
  ub4 pc = 0;
  ub4 x;

  while (prog[pc] != hi32) {
    x = prog[pc];
    info("%2u %x",pc,x);
    pc++;
  }

  lvarmem[Nvar] = n;
  lvarmem[Avar] = 0;

  ub8 T0=0;

  timeit(&T0,nil);

  runir(prog,lvarmem);

  timeit2(&T0,n,"runit");

  vx = 0;
  while (n) {
    vx += 2;
    n--;
  }
  timeit2(&T0,vx,"runit");

  info("nvar %u avar %u",lvarmem[Nvar],lvarmem[Avar]);
}

static int wrfile(void)
{
  ub2 pos;
  ub2 blen = 1024;
  char buf[1024];

  static struct bufile sfp;
  ub4 bulen = 4096;

  sfp.nam = "runvm.out";
  sfp.dobck = 2;

  myfopen(&sfp,bulen,1);

  return myfclose(&sfp);
}

enum Cmdopt { Co_until=1,Co_uncond,Co_Winfo };

static struct cmdopt cmdopts[] = {
//  { "trace",'t',Co_trace,"?%ulevel","enable tracing" },
//  { "Winfo",' ',Co_Winfo,"list","comma-separated list of diags to report as info" },
  { "",'u',      Co_uncond, "",         "unconditional write" },
  { "until", ' ',Co_until, "%espec,gen,out", "process until <pass>" },
  { nil,3,0,"<spec> <syntab> <syndef>","vmrun"}
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
  } else prgnam = "vmrun";

  globs.prgnam = prgnam;
  globs.msglvl = Info;
  globs.rununtil = 0xff;
  globs.resusg = 1;

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
    if (!bcfname) bcfname = (cchar *)*argv;
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
//  else if (!irdefname) { errorfln(FLN,0,"missing ir def file"); return 1; }

  sassert(sizeof(ub8) == 8,"expect long long to be 8 bytes");

  dotest();

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

#endif
