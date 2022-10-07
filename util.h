/* util.h - generic utilities

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

struct cmdopt { // commandline arg defines
  const char *lname;
  const unsigned char sname;
  ub2 opt;
  const char *arg;
  const char *desc;
};

struct cmdval { // matched commandline arg
  struct cmdopt *op;
  ub4 uval;
  const char *sval;
  char lead; // - or +
  ub2 olen,vlen;
  ub4 err;
};

struct bufile {
  cchar *nam;
  ub1 *buf;
  int fd;
  ub4 pos,top;
  ub4 len;
  ub4 fln;
  ub1 dobck;
  bool perm,err;
};

enum Parsearg { Pa_nil,Pa_eof,Pa_plusmin,Pa_plus1,Pa_min1,Pa_min2,Pa_found,Pa_found2,Pa_genfound,Pa_genfound2,Pa_notfound,Pa_regarg,Pa_noarg };
enum Genopts { Co_version=0x1000, Co_verbose,Co_errwarn,Co_quiet,Co_resusg,Co_help,Co_license, Co_dry };

#define fileopen(name,must) fileopenfln(FLN,(name),(must))
#define filewrite(fd,buf,len,name) filewritefln(FLN,(fd),(buf),(len),(name))

extern int filecreate(const char *name,int mustsucceed);
extern int fileappend(const char *name);
extern int fileopenfln(ub4 fln,const char *name,int mustexist);
extern int filewritefln(ub4 fln,int fd, const void *buf,ub8 len,const char *name);
extern int fileread(int fd,void *buf,ub4 len,const char *name);
extern int fileclose(int fd,const char *name);
extern int fileexists(const char *name);
extern int fileremove(const char *name);
extern int filerotate(const char *name,const char old,const char new);

extern int readfile(struct myfile *mf,const char *name, int mustexist,ub4 maxlen);
extern int readfile_pad(struct myfile *mf,const char *name, int mustexist,ub4 maxlen,ub4 padlen,const char pad);
extern int writefile(const char *name,char *buf,ub4 len);

extern int readpath(struct myfile *mf,const char *dir,const char *name, int mustexist,ub4 maxlen);
extern int freefile(struct myfile *mf);
extern int filebck(cchar *name);

extern void myfopen(ub4 fln,struct bufile *f,ub4 len,bool perm);
extern ub4 myfwrite(struct bufile *f,const ub1 *src,ub4 n);
extern void myfputc(struct bufile *f,ub1 c);
extern void myfputs(struct bufile *f,cchar *s);
extern void myfputsfln(ub4 fln,struct bufile *f,cchar *s);
extern ub4 myfprintf(struct bufile *f,const char *fmt,...) __attribute__ ((format (printf,2,3)));
extern int myfclose(struct bufile *f);

extern ub4 truncutf8(const char *s,ub4 len);

extern ub4 sort8(ub8 *p,ub4 n,ub4 fln,const char *desc);
extern ub4 fsort8(ub8 *p,ub4 n,int partial,ub4 fln,const char *desc);
extern ub4 sort4(ub4 *p,ub4 n,ub4 fln,const char *desc);
extern ub4 bsearch4(ub4 *p,ub4 n,ub4 key,ub4 fln,cchar *desc);
// extern ub4 bsearch8(ub8 *p,ub4 n,ub8 key,ub4 fln,const char *desc);

extern ub4 *mklntab(cchar *p,ub4 n,ub4 *pcnt);

extern ub4 parse_version(cchar *p,ub1 len);
extern ub1 check_version(ub4 chk,ub4 ver);

extern void prepopts(struct cmdopt *opts,ub2 *lut,bool clr);
extern enum Parsearg parseargs(ub4 argc,char *argv[],struct cmdopt *opts,struct cmdval *cv,ub2 *lut,bool min1long);

extern void usage(struct cmdopt *opts);

extern void iniprgtim(void);

extern void iniutil(void);
extern void exiutil(void);
