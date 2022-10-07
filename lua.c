/* lua.c - lua bytecode compiler main program

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

/* Main driver : parse comandline and roll the ball
 */

#include <stddef.h>
#include <string.h>

#include "base.h"

#include "mem.h"

#include "os.h"

static ub4 msgfile = Shsrc_main;
#include "msg.h"

#include "dia.h"

#include "util.h"

#include "pre.h"
#include "lexsyn.h"
#include "lex.h"

#include "syndef.h"

#include "astyp.h"
#include "synast.h"

extern int syn(struct lexsyn *lsp,struct synast *sa,ub8 T0);

static bool do_chkmem = 0;

struct globs globs;

enum Cmdopt { Co_until=1,Co_prog,Co_emit,Co_trace,Co_noabr,Co_erabr,Co_pretty,Co_runast,Co_nocol,Co_include,
  Co_Werror,Co_Wwarn,Co_Winfo,Co_Wtrace };

static ub2 msgopt = Msg_shcoord | Msg_lno | Msg_col | Msg_Lvl;

#define Incdir 64
static const char *incdirs[Incdir];
static ub2 incdircnt;

static int docc(cchar *src,ub4 slen,bool isfile)
{
  int rv;
  ub8 T0 = 0,T1 = 0;
  struct ast *ap;
  enum Inctyp inc;
  struct prelex pls;
  struct lexsyn ls;

  memset(&pls,0,sizeof(pls));
  memset(&ls,0,sizeof(ls));

  pls.incdircnt = incdircnt;
  pls.incdirs = incdirs;

  inipre();

  timeit(&T0,nil);

  if (isfile) {
    vrb("compile from file %.32s",src);
    inc = Inone;
  } else {
    vrb("compile from cmdline len %u '%.16s%s'",slen,src,slen > 16 ? "..." : "");
    inc = Icmd;
  }
  rv = prelex(src,inc,&pls,T0);
  if (rv) return rv;

  if (globs.rununtil == 2) { info("until prelex %u",globs.rununtil); return 0; }

  if (pls.tkcnt == 0) {
    info("%s is empty",isfile ? src : "cmdline");
    return 0;
  }

  rv = lex(&pls,&ls,T0);
  if (rv) return rv;

  if (globs.rununtil == 3) { info("until lex %u",globs.rununtil); return 0; }

  if (ls.tkcnt == 0) {
    info("%s is empty",isfile ? src : "cmdline");
    return 0;
  }

  if (inisyn()) return 1;
  iniast();

  struct synast *sa = minalloc(sizeof(struct synast),8,0,"synast");

  timeit(&T1,nil);

  rv = syn(&ls,sa,T0);

  timeit2(&T1,ls.srclen,"parse took ");

  timeit2(&T0,ls.srclen,"lex + parse took ");

  if (rv) return rv;

  if (sa->aidcnt == 0) {
    info("%s is empty",ls.name);
    return 0;
  }

  if (globs.rununtil == 4) { info("until syn %u",globs.rununtil); return 0; }

  ap = mkast(sa);

  timeit2(&T1,ls.srclen,"ast took ");

  if (rv) return 1;

//  afree(ls.tkbas,"lex tokens",nextcnt);

  return 0;
}

static struct cmdopt cmdopts[] = {
  { "",        'c', Co_prog,    "prog", "program to run as string" },
  { "emit",    ' ', Co_emit,    "%epass,lex,syn,ast,sem","intermediate pass output to emit" },
  { "pretty-print",'P',Co_pretty,nil,   "pretty print" },
  { "trace",   ' ', Co_trace,    "%epass,lex,syn,ast,sem","pass to trace" },
  { "runast",  ' ', Co_runast,   nil,   "run aka evaluate ast" },
  { "until",   ' ', Co_until,   "%einit,file,lex1,lex,syn,ast", "process until <pass>" },
  { "nocol",   ' ', Co_nocol,   nil,    "line numbers only, no columns for diags" },
  { "noabr",   ' ', Co_noabr,   nil,    "No automatic bug report" },
  { "error-abr",   ' ', Co_erabr,   nil,    "Automatic bug report for all errors" },

  { "include", 'I', Co_include, "dir",  "add directory to include search path" },

  { "Werror",  ' ', Co_Werror,  "list", "comma-separated list of diags to report as error" },
  { "Wwarn",   ' ', Co_Wwarn,   "list", "comma-separated list of diags to report as warning" },
  { "Winfo",   ' ', Co_Winfo,   "list", "comma-separated list of diags to report as info" },

  { "Wtrace",  ' ', Co_Wtrace,  "list", "comma-separated list of diags to report as trace" },

  { "Lua",    0,0,"<srcfile>",nil},
  { "pass",   0,0,"lex, syn, ast","lexical, syntax, AST" },
  { nil,0,0,nil,nil}
};

static void modinfo(void)
{
  cchar *l = lex_info();
  cchar *s = syn_info();

  genmsgfln(0,Info,"%s\n%s\n",l,s);
}

static cchar *srcnam;
static cchar *cmdprog;
static ub2 cmdprglen;

static int cmdline(int argc, char *argv[])
{
  ub4 orgargc = (ub4)argc;
  struct cmdval coval;
  struct cmdopt *op;
  enum Parsearg pa;
  cchar *prgnam;
  bool haveopt,havereg,endopt;
  ub2 ev=0;
  ub4 uval;
  cchar *sval;

  ub2 cmdlut[256];

  if (argc > 0) {
    prgnam = strrchr(argv[0],'/');
    if (prgnam) prgnam++; else prgnam = argv[0];
    argc--; argv++;
  } else prgnam = "lua";

  globs.prgnam = prgnam;
  globs.msglvl = Info;
  globs.rununtil = 0xff;

  iniutil();

  prepopts(cmdopts,cmdlut,1);

  while (argc) { // options
    haveopt = 0;
    havereg = 0;
    endopt = 0;
    pa = parseargs((ub4)argc,argv,cmdopts,&coval,cmdlut,1);

    switch (pa) {
    case Pa_nil:
    case Pa_eof:   endopt = 1; break;

    case Pa_min2:  endopt = 1; break;

    case Pa_min1:
    case Pa_plusmin:
    case Pa_plus1: havereg = 1; break;

    case Pa_regarg:havereg = 1; break; // first non-option regular

    case Pa_notfound:
      error("option '%.*s' at arg %u not found",coval.olen,*argv,orgargc - (ub4)argc);
      return 1;

    case Pa_noarg:
      error("option '%.*s' missing arg",coval.olen,*argv);
      return 1;

    case Pa_found:
    case Pa_genfound:
      if (coval.err) { errorfln(FLN,coval.err,"option '%.*s' invalid arg",coval.olen,*argv); return 1; }
      haveopt = 1;
      argc--; argv++;
      break;
    case Pa_found2:
    case Pa_genfound2:
      if (coval.err) { error("option '%.*s' invalid arg",coval.olen,*argv); return 1; }
      haveopt = 1;
      argc -=2;
      argv += 2;
      break;
    }

    if (havereg) break;
    else if (endopt) { argc--; argv++; break; }

    else if (haveopt) {
      op = coval.op;

      uval = coval.uval;
      sval = coval.sval;
      if (op->opt >= Co_emit && op->opt <= Co_trace) {
        ev = uval ? 1U << (uval-1) : 0x7fff;
      }
      switch(op->opt) {
      case Co_help:   return 1;
      case Co_license:return 1;
      case Co_version:if (globs.msglvl >= Info) modinfo(); return 1;
      case Co_verbose:
      case Co_dry:    break;

      case Co_include:if (incdircnt < Incdir) incdirs[incdircnt++] = sval;
                      else warning("Exceeding %u inc dir limit",Incdir);
                      break;

      case Co_prog:   cmdprog = sval; cmdprglen = coval.vlen; break;
      case Co_until:  globs.rununtil = uval; break;
      case Co_runast: globs.runast = 1; break;
      case Co_emit:   globs.emit  |= ev; break;
      case Co_trace:  globs.trace |= ev; break;
      case Co_pretty: globs.emit  |= 0x8000; break;
      case Co_nocol:  globs.nocol = 1; break;

      case Co_noabr:  globs.abr = 0; break;
      case Co_erabr:  globs.abr = Warn; break;

#if 0
      case Co_Werror: diaset(Error,sval); break;
      case Co_Wwarn:  diaset(Warn, sval); break;
      case Co_Winfo:  diaset(Info, sval); break;
      case Co_Wtrace: diaset(Vrb,  sval); break;
#endif
      }
    }
  }

  while (argc) { // regular args
    if (!srcnam) srcnam = *argv;
    argc--; argv++;
  }

  setmsglvl(globs.msglvl,msgopt);
  return 0;
}

static int init(void)
{
  inios();
  globs.maxvm = 4;
  oslimits();
  inimem();
  setsigs();

  inimsg(msgopt);

  return 0;
}

static void myexit(void)
{
  if (do_chkmem) achkfree();
  eximsg();
  exios(1);
  eximem(1);
}

int main(int argc, char *argv[])
{
  int rv;

  init();

  rv = cmdline(argc,argv);
  if (rv) {
    eximsg();
    return 1;
  }

  if (globs.rununtil == 0) { genmsgfln(0,Info,"until cmd"); return 0; }

  inilex();

  if (cmdprog) {
    rv = docc(cmdprog,cmdprglen,0);
  } else if (srcnam) {
    if (*srcnam == 0) { errorfln(FLN,0,"empty script name"); return 1; }
    rv = docc(srcnam,0,0);
  } else {
    errorfln(FLN,0,"script file or script arg expected");
    return 1;
  }

  myexit();

  return rv || globs.retval;
  lastcnt
}
