/* report.c - handle bug reports

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

// #include <stddef.h>
#include <string.h>

#include "../base.h"

//#include "mem.h"

#include "../net.h"

#include "../os.h"

#include "../fmt.h"

static ub4 msgfile = Shsrc_report;
#include "../msg.h"

#include "../tim.h"

#include "../util.h"

struct globs globs;

enum Cmdopt { Co_port=1,Co_fg,Co_trace };

static ub2 msgopt = Msg_shcoord | Msg_lno | Msg_col | Msg_Lvl | Msg_tim;

static struct cmdopt cmdopts[] = {
  { "trace",   't', Co_trace,    nil, "enable trace" },
  { "foreground",   'f', Co_fg,   nil,    "run in foreground" },
  { "port",   'p', Co_port,   "%uport",    "UDP port to listen" },

  { "Report",0,0,"dir", "directory for report output"},
  { "todo",   0,0,"todo", "todo" },
  { nil,0,0,nil,nil}
};

static const char *dstdir = ".";
static ub2 port = 8000;
static bool foreground;
static bool trace;

static void msgloop(void)
{
  info("listening on port %u",port);
  ub2 len = 1024;
  char buf[1024];
  char fnam[1024];
  int netfd = netsocket(0);
  int filfd;
  int nr;
  ub8 ts;

  netbind(netfd,port);

  do {
    nr = netrcv(netfd,buf,len);
    ts = daytime_usec();
    mysnprintf(fnam,0,1024,"%s/%s.txt",dstdir,fmtusec(ts));
    filfd = oscreate(fnam);
    oswrite(filfd,buf,(ub4)nr,nil);
    osclose(filfd);
  } while (1);
}

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
  } else prgnam = "report";

  globs.prgnam = prgnam;
  globs.msglvl = Info;

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

      switch(op->opt) {
      case Co_help:   return 1;
      case Co_license:return 1;
      case Co_version:return 1;
      case Co_verbose:
      case Co_dry:    break;

      case Co_fg:     foreground = 1; break;
      case Co_trace:  trace = 1; break;
      case Co_port:   port = uval; break;
      }
    }
  }

  if (argc) { // regular args
    dstdir = *argv;
    argc--; argv++;
  }

  setmsglvl(globs.msglvl,msgopt);
  return 0;
}

static int init(void)
{
  globs.abr = 0;
  globs.maxvm = 4;
  inios();

  oslimits();
  setsigs();

  inimsg(msgopt);
  setmsgbuf(0);

  return 0;
}

static void myexit(void)
{
  eximsg();
  exios(1);
}

void bugreport(cchar *rep,ub2 rlen,cchar *tag) {}

int main(int argc, char *argv[])
{
  int rv;

  init();

  rv = cmdline(argc,argv);
  if (rv) {
    eximsg();
    return 1;
  }

  msgloop();

  myexit();

  return rv || globs.retval;
}
#endif
