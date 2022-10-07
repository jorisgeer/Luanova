/* msg.c - log and diagnostic messages

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

/* This file contains messaging logic used for :
   - diagnostics relating to the compiland e.g. syntax error in source
   - diagnostics relating to the language processor e.g. internal error
 */

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "base.h"

#include "mem.h"

#include "fmt.h"

static ub4 msgfile = Shsrc_msg;
#include "msg.h"

#include "os.h"

#include "tim.h"

#include "util.h"

extern void bugreport(cchar *buf,ub2 len,cchar *tag);

static inline ub4 strlen4(cchar *s) { return (ub4)strlen(s); }

static enum Msglvl msglvl = Info;
static enum Msgopts orgopts,msgopts = Msg_shcoord; // | Msg_tim;
static bool nobuffer = 0;
static bool ena_color;

static int ttyfd = 1;
static int logfd = -1;

static ub8 T0;

static ub4 warncnt,errcnt,assertcnt,fatalcnt;

#define Msglen 2048
#define Msgbuf 4096

ub4 msgwarncnt(void) { return warncnt; }

ub4 msgerrcnt(void)
{
  ub4 cnt = errcnt + assertcnt;

  return cnt;
}

static void os_write(int fd,cchar *buf,ub4 len) { oswrite(fd,buf,len,nil); }

static void dowritefd(int fd,const char *buf,ub4 len)
{
  int rv;

  if (len == 0) { os_write(fd,"\nnil msg\n",9); return; }

  ub4 prvi=0;

#if 0
  for (i = 0; i < len; i++) {
    if (buf[i] == 0)  {
      if (prvi < i) os_write(fd,buf+prvi,i - prvi);
      prvi = i+1;
      os_write(fd," \\x00 ",6);
    }
  }
  if (prvi >= len) return;
#endif

  rv = oswrite(fd,buf+prvi,len-prvi,nil);

  if (rv < 0) {
    os_write(2,"\nI/O error on msg write\n",24);
    errcnt++;
  }
}

static void dowrite(const char *buf,ub4 len)
{
  if (logfd != -1) dowritefd(logfd,buf,len);
  else dowritefd(ttyfd,buf,len);
}

static ub4 msgseq,errseq,warnseq;

static char msgbuf[Msgbuf];
static ub4 bufpos,buftop = Msgbuf;

static char lastwarn[128];
static char lasterr[128];

static void flsbuf(void)
{
  if (bufpos) {
    dowrite(msgbuf,bufpos);
    bufpos = 0;
  }
}

void msgfls(void) { flsbuf(); }

static void msgwrite(cchar *p,ub4 len)
{
  ub4 n;

  if (nobuffer) { dowrite(p,len); return; }

  while (bufpos + len > buftop) {
    n = buftop - bufpos;
    memcpy(msgbuf + bufpos,p,n); p += n; len -= n;
    dowrite(msgbuf,buftop);
    bufpos = 0;
  }
  if (len) { memcpy(msgbuf + bufpos,p,len); bufpos += len; }
}

void msg_write(const char *buf,ub4 len) { msgwrite(buf,len); }

static void msg_swrite(const char *buf)
{
  if (buf) msgwrite(buf,strlen4(buf));
}

static const char *shflnames[Shsrc_count] = {
  [Shsrc_ast]    = "ast",
  [Shsrc_base]   = "base",
  [Shsrc_dia]    = "dia",
  [Shsrc_eval]   = "eval",
  [Shsrc_mem]    = "mem",
  [Shsrc_msg]    = "msg",
  [Shsrc_lex]    = "lex",
  [Shsrc_lex1]   = "lextab",
  [Shsrc_pre]    = "pre",
  [Shsrc_genir]  = "genir",
  [Shsrc_genlex] = "genlex",
  [Shsrc_gensyn] = "gensyn",
  [Shsrc_os]     = "os",
  [Shsrc_syn]    = "syn",
  [Shsrc_main]   = "lua",
  [Shsrc_net]    = "net",
  [Shsrc_time]   = "time",
  [Shsrc_report] = "report",
  [Shsrc_util]   = "util",
  [Shsrc_vm]     = "vm",
  [Shsrc_vmrun]  = "vmrun"
};

static void msg_wrfln(ub4 fln)
{
  char buf[64];

  if (fln == 0 || fln == hi32) return;

  ub4 lno = fln & 0xffff;
  ub4 fileno = fln >> 16;
  ub4 n;

  const char *name = fileno < Shsrc_count ? shflnames[fileno] : "???";

  n = mysnprintf(buf,0,64,"%6s.%-4u\n",name,lno);

  msg_write(buf,n);
}

void msg_errwrite(ub4 fln,ub4 fln2,const char *buf)
{
  msg_swrite("\n");
  msg_wrfln(fln);
  msg_wrfln(fln2);
  msg_swrite(" error: ");

  if (buf) msg_swrite(buf);
}

static void msginfo(ub4 shfln)
{
  globs.shfln = shfln;
}

//                                    1         2         3         4
//                          0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2
static cchar lvlnams[]   = "fatal assert error warn info vrb vrb2 Nolvl";
static ub1 lvlpos[Nolvl+1] = {0,6,13,19,24,29,33,38};

//                         f  a  e  w
static ub1 lvlclr[Info] = {36,35,34,33};

#define Clr0 "\x1b[%um"
#define Clr1 "\x1b[0m"

/* main message printer.
 * if ap is nil, print fmt as %s
 */
static void msgps(ub4 shfln,enum Msglvl lvl,cchar *srcnam,ub4 lno,ub4 col,cchar *fmt,va_list ap,cchar *pfx,cchar *sfx)
{
  ub4 n,pos = 0,maxlen = Msglen - 1;
  ub4 shfno=0,shlno=0;
  ub1 c,emp = 0xff;
  cchar *shfnam = nil;
  cchar *lvlnam = "";
  cchar *p;
  char buf[Msglen];
  char msgbuf2[128];
  char coords[64];
  char *coordp = coords + 64;
  enum Msgopts dotim = msgopts & Msg_tim;
  ub4 ec;
  ub8 T1,dt=0;

  if (dotim) {
    T1 = gettime_usec();
    dt = T1 - T0;
  }

  if (fmt == nil) {
    fmt = "(nil fmt)"; ap = nil; lvl = Fatal;
  } else if ((size_t)fmt < 4096) {
    fmtstring(msgbuf2,"(int fmt 0x%x)",(ub4)(ub8)fmt);
    fmt = msgbuf2; ap = nil; lvl = Fatal;
  }

  if (shfln && shfln != hi32) {
    if (shfln & Bit31) {
      shfln &= ~Bit31;
      emp = 0xdf;
    }
    shfno = shfln >> 16;
    shlno = shfln & hi16;

    shfnam = shfno < Shsrc_count ? shflnames[shfno] : "???";
    if (shfnam == nil) shfnam = "???";
  }

  switch (lvl) {
    case Fatal:   errcnt++;    break;
    case Assert:  assertcnt++; break;
    case Error:   errcnt++;    break;
    case Warn:    warncnt++;   break;
    case Info:    break;
    case Vrb:     break;
    case Vrb2:    break;
    case Nolvl:   break;
    default:      errcnt++; lvl = Error;
  }
  lvlnam = lvlnams + lvlpos[lvl];

  if (dotim) pos += mysnprintf(buf,pos,maxlen,"%4u ",(ub4)dt);

  if (shfln && shfln != hi32 && (msgopts & Msg_shcoord)) pos += mysnprintf(buf,pos,maxlen,"%7s.%-4u ",shfnam,shlno);

  if (msgopts & Msg_Lvl) {
    c = *lvlnam & emp;
    if (lvl < Info && ena_color) pos += mysnprintf(buf,pos,maxlen,Clr0 "%c" Clr1,lvlclr[lvl],c);
    else buf[pos++] = c;
    buf[pos++] = ' ';
  }

  if (lvl < Info || (msgopts & Msg_lvl)) pos += mysnprintf(buf,pos,maxlen,"%-8s`z ",lvlnam);

  if (srcnam) {
    if (msgopts & Msg_fno) {
      pos += mysnprintf(buf,pos,maxlen,"%s ",srcnam);
    }
    if (lno && (msgopts & Msg_lno)) {
      *--coordp = 0; *--coordp = ' '; *-- coordp = ':';
      if (col && (msgopts & Msg_col) ) { coordp = utoa(coordp,col); *--coordp = '.'; }
      coordp = utoa(coordp,lno);
      pos += mysnprintf(buf,pos,maxlen,"%-8s",coordp);
    }
  }

  if (pfx) {
    n=0;
    while (pfx[n] && pos + 2 < maxlen) buf[pos++] = pfx[n++];
    buf[pos++] = ' ';
  }

  if (ap) pos += myvsnprint(buf,pos,maxlen,fmt,ap);
  else { while (pos < maxlen && *fmt) buf[pos++] = *fmt++; }

  if (sfx) {
    if (pos + 1 < maxlen) buf[pos++] = ' ';
    while (*sfx && pos + 1 < maxlen) buf[pos++] = *sfx++;
  }
  buf[pos++] = '\n';

  if (lvl < Info && logfd != -1) {
    flsbuf();
    dowritefd(ttyfd,buf,pos);
  }

  msgwrite(buf,pos);

  if (pos >= maxlen) {
    flsbuf();
    logfd = -1;
    msg_errwrite(shfln,FLN,"\nerror: buffer full at above message\n");
    msg_swrite(fmt);
    flsbuf();
  }

  if (lvl == Warn) {
    memcpy(lastwarn,buf,n=min(pos-1,sizeof(lastwarn)-1));
    lastwarn[n] = 0;
    warnseq = msgseq;
  } else if (lvl < Warn) {
    memcpy(lasterr,buf,n=min(pos-1,sizeof(lasterr)-1));
    lasterr[n] = 0;
    errseq = msgseq;
    flsbuf();
  }
  msgseq++;

  if (lvl >= globs.abr) return;

  ub8 ts = daytime_msec();
  cchar *tsf = fmtmsec(ts);
  char tag[128];

  // re-format canonically for bug report
  if (pfx == 0) {
    pfx = msgbuf2;
    msgbuf2[0] = *lvlnam & 0xdf;
    msgbuf2[1] = 0;
  }
  mysnprintf(tag,0,128,"%.8s-%s.%u-%s",pfx,shfnam,shlno,tsf);

  pos = mysnprintf(buf,0,1024,fmt,ap);

  bugreport(buf,pos,tag);
}

static void msg(ub4 shfln,enum Msglvl lvl,cchar *srcnam,ub4 lno,ub4 col,cchar *fmt,va_list ap)
{
  msgps(shfln,lvl,srcnam,lno,col,fmt,ap,nil,nil);
}

void msglog(cchar *fnam,cchar *fext,cchar *desc)
{
  char path[Fname];
  ub1 x;

  if (logfd >= 0) {
    info("%s %s intermediate code dump",fatalcnt ? "interrupted" : "end",desc);
    msgopts = orgopts;
    flsbuf();
    osclose(logfd);
    logfd = -1;
    if (fatalcnt == 0) info("done emitting %s intermediate code",desc);
    flsbuf();
  }
  if (fnam == nil || *fnam == 0) {
    return;
  }

  fmtstring(path,"%s.%s",fnam,fext);
  filebck(path);

  info("emitting %s intermediate code in %s",desc,path);
  flsbuf();
  logfd = oscreate(path);
  if (logfd == -1) { error("cannot create %s: %m",path); return; }
  orgopts = msgopts;
  x = (ub1)msgopts & ~ (ub1)(Msg_shcoord|Msg_tim|Msg_fno|Msg_Lvl);
  msgopts = (enum Msgopts)x;
  info("%s - intermediate %s code dump\n",path,desc);
}

/* returns file name,line,col given 32-bit linear pos
 * uses file and line table
 * interprets pos as line if no line table
 */
static const char *getsrcpos(ub4 fpos,ub4 *plno,ub4 *pcol,ub4 *pparfpos)
{
  ub4 n,pos,len,lno,col,fpos0;
  ub4 lncnt=0,*lntab=nil;
  ub4 filcnt;
  bool linonly=0;

  *plno = 0; *pcol = 0;
  *pparfpos = hi32;

  if (fpos == hi32) return nil;
  else if (fpos & Lno) { linonly = 1; fpos &= ~Lno; }

  pos = fpos;
  len = 0;
  if (lntab == nil || linonly) { *plno = fpos; return ""; } // pass line numbers directly if no lntab

  if (pos >= len) {
    if (pos - len > 4) warning("invalid pos %u.%4x above %u",pos,pos,len);
    lno = lncnt; col = 0;
  } else if (lncnt < 2) lno = 0;
  else {
    lno = bsearch4(lntab,lncnt,pos,FLN,"srcpos");
    if (lno && lntab[lno] > pos) lno--;
  }
  if (lno) {
    if (pos < lntab[lno]) { errorfln(FLN,0,"invalid pos %u below line %u org %u",pos,lno,lntab[lno]); return ""; }
    col = pos - lntab[lno];
  } else col = pos;
  col++;
  lno++;
  *plno = lno; *pcol = col;

  return "";
}

ub4 getsrcln(ub4 fpos)
{
  ub4 par,col,lno;

  getsrcpos(fpos,&lno,&col,&par);
  return lno;
}

void vpmsg(ub4 shfln,enum Msglvl lvl,const char *srcnam,ub4 lno,ub4 col,const char *fmt,va_list ap,const char *suffix)
{
  if (msglvl >= lvl || lvl >= Nolvl) msgps(shfln,lvl,srcnam,lno,col,fmt,ap,nil,suffix);
}

static void vmsgint(ub4 shfln,enum Msglvl lvl,ub4 fpos,cchar *fmt,va_list ap,cchar *pfx,cchar *sfx)
{
  ub4 lno,col;
  ub4 parfpos = hi32;
  cchar *name = nil;

  if (fpos != hi32) {
    name = getsrcpos(fpos,&lno,&col,&parfpos);
    if (name == nil) errorfln(FLN,shfln,"nil name for fpos %u",fpos);
  } else { name = nil; lno = col = 0; }

  msgps(shfln,lvl,name,lno,col,fmt,ap,pfx,sfx);

  while (parfpos != hi32) {
    if (parfpos >= fpos) { errorfln(FLN,0,"invalid parent pos %u above %u",parfpos,fpos); return; }
    fpos = parfpos;
    name = getsrcpos(fpos,&lno,&col,&parfpos);
    msg(Info,shfln,name,lno,col,"included",nil);
  }
}

void vmsg(ub4 shfln,enum Msglvl lvl,ub4 fpos,cchar *fmt,va_list ap)
{
  if (msglvl >= lvl || lvl >= Nolvl) vmsgint(shfln,lvl,fpos,fmt,ap,nil,nil);
}

void vmsgps(ub4 shfln,enum Msglvl lvl,ub4 fpx,cchar *fmt,va_list ap,cchar *pfx,cchar *sfx)
{
  if (msglvl < lvl && lvl < Nolvl) return;
  ub4 fps = fpx & hi24;
  enum Msgopts orgo = msgopts;

  if (fpx & Bit31) msgopts = (fpx >> 24) & 0x7f;
  vmsgint(shfln,lvl,fps,fmt,ap,pfx,sfx);
  msgopts = orgo;
}

void __attribute__ ((format (printf,3,4))) genmsgfln(ub4 fln,enum Msglvl lvl,const char *fmt,...)
{
  va_list ap;

  msginfo(fln);

  if (msglvl < lvl) return;

  va_start(ap,fmt);
  msg(fln,lvl,nil,0,0,fmt,ap);
  va_end(ap);
}

void __attribute__ ((format (printf,4,5))) genmsg2fln(ub4 fln,ub4 fln2,enum Msglvl lvl,const char *fmt,...)
{
  va_list ap;

  msginfo(fln);

  if (msglvl < lvl) return;

  msg_wrfln(fln2);

  va_start(ap,fmt);
  msg(fln,lvl,nil,0,0,fmt,ap);
  va_end(ap);
}

void __attribute__ ((format (printf,4,5))) sgenmsgfln(ub4 shfln,ub4 fpos,enum Msglvl lvl,const char *fmt, ...)
{
  va_list ap;

  msginfo(shfln);
  if (msglvl < lvl) return;

  va_start(ap, fmt);
  vmsgint(shfln,lvl,fpos,fmt,ap,nil,nil);
  va_end(ap);
}

void __attribute__ ((format (printf,3,4))) sinfofln(ub4 shfln,ub4 fpos,const char *fmt, ...)
{
  va_list ap;

  msginfo(shfln);
  if (msglvl < Info) return;

  va_start(ap, fmt);
  vmsgint(shfln,Info,fpos,fmt,ap,nil,nil);
  va_end(ap);
}

void __attribute__ ((format (printf,3,4))) swarnfln(ub4 shfln,ub4 fpos,const char *fmt, ...)
{
  va_list ap;

  msginfo(shfln);
  if (msglvl < Warn) return;

  va_start(ap, fmt);
  vmsgint(shfln,Warn,fpos,fmt,ap,nil,nil);
  va_end(ap);
}

Noret void __attribute__ ((format (printf,4,5))) fatalfln(ub4 fln,ub4 fln2,ub4 fpos,const char *fmt, ...)
{
  va_list ap;

  fatalcnt = 1;

  msglog(nil,nil,nil);
  msg_wrfln(fln2);

  va_start(ap, fmt);
  vmsgint(fln,Fatal,fpos,fmt,ap,nil," : exiting");
  va_end(ap);
  doexit(1);
}

Noret void __attribute__ ((format (printf,4,5))) icefln(ub4 fln,ub4 fln2,ub4 fpos,const char *fmt, ...)
{
  va_list ap;

  fatalcnt = 1;

  msglog(nil,nil,nil);
  msg_wrfln(fln2);

  va_start(ap, fmt);
  vmsgint(fln,Fatal,fpos,fmt,ap,"ice"," : exiting");
  va_end(ap);
  doexit(1);
}

Noret void __attribute__ ((format (printf,3,4))) serrorfln(ub4 shfln,ub4 fpos,const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vmsgint(shfln,Error,fpos,fmt,ap,nil,nil);
  va_end(ap);
  doexit(1);
}

void __attribute__ ((format (printf,3,4))) errorfln(ub4 fln,ub4 fln2,const char *fmt, ...)
{
  va_list ap;

  msg_wrfln(fln2);

  va_start(ap,fmt);
  msg(fln,Error,nil,0,0,fmt,ap);
  va_end(ap);
}

Noret void __attribute__ ((format (printf,2,3))) assertfln(ub4 fln,const char *fmt,...)
{
  va_list ap;

  fatalcnt = 1;

  va_start(ap,fmt);
  msgps(fln,Assert,0,0,0,fmt,ap,nil," : exiting");
  va_end(ap);
  doexit(1);
}

void showcntfln(ub4 fln,cchar *nam,ub4 cnt)
{
  ub2 n = 0;
  ub4 pos;
  char c;
  cchar * plr;
  char buf[256];

  if (nam == nil) nam = "(nil)";
  c = *nam;
  if (c == '#') { plr = ""; c = *++nam; }
  else plr = "s";
  if (c == '0') c=*nam++;
  else if (cnt == 0) return;
  if (c >= '0' && c <= '9') { n = c - '0'; nam++; }
  pos = mysnprintf(buf,0,256,"%*u %s%.*s",n,cnt,nam,cnt != 1,plr);
  if (cnt > 99999) pos += mysnprintf(buf,pos,256," (%u`)",cnt);
  genmsgfln(fln,Info,buf,nil);
}

void showscntfln(ub4 fln,ub2 pad,cchar *nam,ub4 cnt)
{
  ub2 n = 0;
  ub4 pos;
  char buf[256];

  if (cnt == 0) return;
  if (nam == nil) nam = "(nil)";
  pos = mysnprintf(buf,0,256,"%*u %s",n,cnt,nam);
  if (cnt > 99999) pos += mysnprintf(buf,pos,256," (%u`)",cnt);
  genmsgfln(fln,Info,buf,nil);
}

void showsizfln(ub4 fln,cchar *nam,ub4 siz)
{
  ub2 n = 0;
  char c;

  if (siz == 0 || nam == nil || (c = *nam) == 0) return;

  if (c >= '0' && c <= '9') n = (ub2)(*nam++ - '0');
  genmsgfln(fln,Info,"%s %*u`",nam,n,siz);
}

ub4 gettime_msec(void)
{
  ub8 t = gettime_usec();
  return t / 1000;
}

void timeit2fln(ub4 fln,ub8 *pt0,ub4 n,cchar *mesg)
{
  ub8 t0u = *pt0;
  ub8 t1u = gettime_usec();
  ub8 dtu,dtm,dts;
  ub8 nn,npx=0;
  ub4 x4,y4=0;
  bool rem = 0;
  cchar *pfx;
  cchar *btk;
  char u;
  static char buf[256];
  ub4 pos;

  *pt0 = t1u;
  if (t0u == 0 || globs.resusg == 0 || mesg == nil || msglvl < Info) return; // start measure

  nn = n;
  dtu = t1u - t0u;
  if (dtu < 1500) { x4 = (ub4)dtu; pfx = "micro"; u = 'u'; npx = nn / max(dtu,1); }
  else {
    pfx = "milli"; u = 'm';
    dtm = dtu / 1000;
    if (dtu < 10000) { x4 = (ub4)dtm; rem = 1; y4 = dtu % 1000; npx = nn / max(dtm,1); }
    else if (dtm >= 1500) {
      pfx = ""; u = ' ';
      dts = dtm / 1000;
      npx = nn / max(dts,1);
      x4 = (ub4)dts; y4 = dtm % 1000; rem = 1;
    } else { x4 = (ub4)dtm; npx = nn / max(dtm,1); }
  }

  btk = strchr(mesg,'`');
  if (btk) {
    pos = 0;
    while (mesg < btk) buf[pos++] = *mesg++;
    pos += mysnprintf(buf,pos,256,"%u`%s %u",n,btk+1,x4);
  } else {
    pos = mysnprintf(buf,0,256,"%s %u",mesg,x4);
  }
  if (rem) pos += mysnprintf(buf,pos,256,".%u",y4 / 10);
  pos += mysnprintf(buf,pos,256," %ssecond%.*s",pfx,x4 != 1,"s");

  if (n > 1024) {
    if (u == 'm') { npx *= 1000; u = ' '; }
    else if (u == 'u') { npx *= 1000000; u = ' '; }
    mysnprintf(buf,pos,256," ~ %u` / %csec",(ub4)npx,u);
  }

  msg(fln,Info,nil,0,0,buf,nil);
}

void timeitfln(ub4 fln,ub8 *pt0,cchar *mesg)
{
  timeit2fln(fln,pt0,0,mesg);
}

// level to be set beforehand
void inimsg(ub2 opts)
{
  msgopts = opts;

  T0 = gettime_usec();
}

void eximsg(void)
{
  ub4 ecnt = msgerrcnt();
  ub4 wcnt = warncnt;

  if (ecnt) {
    if (ecnt > 1 || msgseq - errseq > 5) showcnt("error",ecnt);
    if (msgseq - errseq > 5) info("  last %s",lasterr);
  } else if (wcnt) {
    if (wcnt > 1 || msgseq - warnseq > 5) showcnt("warning",wcnt);
    if (msgseq - warnseq > 5) info("  last %s",lastwarn);
  }

  if (wcnt && globs.errwarn && globs.retval == 0) {
    globs.retval = 1;
    genmsgfln(FLN,Info,"Treating warnings as error");
  }
  flsbuf();
  nobuffer = 1;
}

void setmsgbuf(bool ena)
{
  if (ena == 0) nobuffer = 1;
}

void setmsglvl(enum Msglvl lvl,ub2 opts)
{
  msglvl = lvl;
  msgopts = opts;
  if (nobuffer) genmsgfln(FLN,Info,"message buffer disabled");
  if (lvl >= Vrb) info("message level %u",lvl);
}

enum Msglvl getmsglvl(void) { return msglvl; }
