/* mem.h

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

#define MFLN (__LINE__|(ub4)(msgfile << 16))

#define Mnofil 0x100
#define Mo_ok0 0x200
#define Mnores 0x400

struct expmem {
  ub1 *bas;
  ub4 pos,top; // in elsiz units
  ub4 inc;
  ub2 elsiz;
  ub2 align;
  ub2 ini;
  ub2 mapcnt;
  bool min;
  char desc[16];
};

struct mempart {
  ub4 nel,siz;
  void *ptr;
  cchar *dsc;
  ub2 fil;
  ub2 algn;
};

#define alloc(cnt,el,fil,desc,cntr) (el*)alloc_fln(MFLN,(cnt),sizeof(el),(fil),desc,cntr)
#define afree(ptr,desc,cntr) afree_fln(MFLN,(void *)(ptr),desc,cntr)
#define afree0(ptr,desc,cntr) afree0_fln(MFLN,(ptr),desc,cntr)

#define allocset(part,cnt,fil,desc,cntr) allocset_fln(MFLN,part,cnt,fil,desc,cntr)

#define minalloc(n,a,fil,dsc) minalloc_fln(MFLN,(n),(a),(fil),dsc)

#define blkexp(xp,cnt,typ) blkexp_fln(FLN,(xp),(cnt),sizeof(typ))
#define blkptr(xp,itm,typ) (typ *)blk_ptr(xp,itm)

#if defined __GNUC__ || defined  __clang__
  #define Memdesc 64
  #define nextcnt __COUNTER__
  #define lastcnt sassert(__COUNTER__ < Memdesc,"expect anchor count < Memdesc");
#else
  #define Memdesc 3000
  #define nextcnt __LINE__
  #define lastcnt sassert(__LINE__ < Memdesc,"expect anchor count < Memdesc");
#endif

extern Mallike void *myalloc(ub4 len);
extern Mallike void *remalloc(void *p,ub4 len);
extern void mfree(void *p);

extern Mallike void *alloc_fln(ub4 fln,ub8 nelem,ub4 elsiz,ub2 fil,const char *desc,ub2 counter);
extern void afree_fln(ub4 fln,void *p,const char *desc,ub2 counter);
extern void afree0_fln(ub4 fln,void *p,const char *desc,ub2 counter);

extern void *allocset_fln(ub4 fln,struct mempart *parts,ub2 npart,ub2 fil,const char *desc,ub2 counter);

extern Mallike void *minalloc_fln(ub4 fln,ub4 n,ub2 align,ub2 fil,cchar *desc);

extern ub4 blkexp_fln(ub4 fln,struct expmem *xp,ub4 cnt,ub4 typsiz);
extern ub1 *blk_ptr(struct expmem *xp,ub4 itm);

extern void achkfree(void);

extern ub4 meminfo(void);
extern int memrdonly(void *p,ub8 len);
extern void *nearblock(void *p);

extern void memcfg(ub4 maxvm);

extern void showmemsums(void);

extern void inimem(void);
extern void eximem(bool show);
