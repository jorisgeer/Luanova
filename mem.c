/* mem.c - memory allocation wrappers and provisions

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

/* Memory allocation:
   wrappers, pool allocators
   The main wrapper alloc() passes the request on, depending on size to :
   minalloc() - disposable tiny items
   malloc(3) - regular items
   mmap(2) - larger items

   In the latter 2 cases, bookeeping outside the allocated block is used for basic double-free and leak detection

 */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "base.h"

#include "os.h"

static ub4 msgfile = Shsrc_mem;
#include "mem.h"

#include "msg.h"

/*

  buddy for 256 - 1m

  mmap >= 1m
 */

#ifdef VALGRIND
 #include "../valgrind/valgrind.h"
 static ub2 redzone = 64;
#else
 static ub2 redzone = 0;
 #define VALGRIND_MALLOCLIKE_BLOCK(p,n,red,zero)
#endif

#undef Diralloc

#undef fatal
#define fatal(fln,fmt,...) fatalfln(FLN,fln,hi32,fmt,__VA_ARGS__)

static const ub4 Maxmem_mb = 8192;

static const ub4 mmap_thres = 65536;

static const ub4 mini_thres = 1024;

static const ub4 maxalign = 16;
static const ub4 stdalign = 8;
static const ub4 minalignmask = 7;

#define Aihshbit1 8
#define Aihshbit2 10

static ub4 pagemask = 4095;

static ub4 aiuses[2];

static ub4 totalkb,maxkb;

static cchar *descs[Memdesc * (Shsrc_mem+1)];
static ub4 flns[Memdesc * (Shsrc_mem+1)];
static ub2 elsizes[Memdesc * (Shsrc_mem+1)];

struct ainfo {
  const void *ptr;
  ub4 len;
  ub2 allocanchor;
  ub2 freeanchor;
};

static struct ainfo aitab1[1U << Aihshbit1];
static struct ainfo aitab2[1U << Aihshbit2];

static void addsum(ub4 b)
{
  ub4 kb = b >> 10;

  totalkb += kb;
  if (totalkb > maxkb) maxkb = totalkb;
}

static void subsum(ub4 b)
{
  ub4 kb = b >> 10;

  totalkb -= min(kb,totalkb);
}

// Thomas Wang 64 bit integer hash
// http://web.archive.org/web/20120720045250/http://www.cris.com/~Ttwang/tech/inthash.htm
#ifdef __clang
__attribute__((no_sanitize("unsigned-integer-overflow")) )
#endif
static ub8 ptrhash(ub8 key)
{
  key = (~key) + (key << 21); // key = (key << 21) - key - 1;
  key = key ^ (key >> 24);
  key = (key + (key << 3)) + (key << 8); // key * 265
  key = key ^ (key >> 14);
  key = (key + (key << 2)) + (key << 4); // key * 21
  key = key ^ (key >> 28);
  key = key + (key << 31);
  return key;
}

static ub8 align8(ub4 fln,ub8 x,ub4 a,cchar *dsc)
{
  ub4 r;

  switch (a) {
  case 0: ice(fln,hi32,"zero align for %lu",x);
  case 1: return x;
  case 2: return x & 1 ? x+1 : x;
  default:
    if (a & (a-1)) ice(fln,hi32,"align %u not power of two for %s",a,dsc);
    else if (a > maxalign) ice(fln,hi32,"align %u above %u for %s",a,maxalign,dsc);
    r = x & ~(a-1);
    return (r == x ? r : r + a);
  }
}

static ub4 align4(ub4 x,ub4 a)
{
  ub4 r;

  switch (a) {
  case 1: return x;
  case 2: return x & 1 ? x+1 : x;
  default:
    r = x & ~(a-1);
    return (r == x ? r : r + a);
  }
}

void *myalloc(ub4 len) { return malloc(len); }

void *remalloc(void *p,ub4 len) { return realloc(p,len); }
void mfree(void *p) { free(p); }

// mini alloc for nonfreeable blocks
static ub4 minchk = 65536;
static void *minpool;
static ub4 minpos,mintop,mintot;
static ub4 mincnt;

void *minalloc_fln(ub4 fln,ub4 n,ub2 a,ub2 fill,cchar *dsc)
{
  ub1 *p;

  genmsg2(fln,Vrb,"minalloc %u@%u for %s",n,a,dsc);

#ifdef Diralloc
  p = malloc(n);
  memset(p,fill,n);
#else
  ub4 inc;

  if (a == 0) {
    genmsg2(fln,Vrb,"minalloc %u align 0 -> %u %s",n,stdalign,dsc);
    a = stdalign;
  } else if (a > maxalign) ice(fln,hi32,"align %u above %u for %s",a,maxalign,dsc);
  else if (a > 2 && (a & (a-1))) ice(fln,hi32,"align %u not power of two for %s",a,dsc);

  if (n == 0) fatal(fln,"zero len for '%s'",dsc);

  minpos = align4(minpos+redzone,a);
  if (minpos + n + redzone > mintop) {
    if (mintop - minpos >= 65536) {
      n += redzone;
      vrb("mmap %u`",n);
      p = osmmap(n,ub1,1);
      return p;
    }
    if (mintop) {
      inc = max(minchk,mintop - (minpos + n + redzone));
      info("inc %u` top %u",inc,mintop);
    } else {
      inc = minchk;
      vrb("inc %u` top %u",inc,mintop);
    }
    inc = nxpwr2(inc,nil);
    genmsg2(fln,Vrb,"inc %u` for %s",inc,dsc);
    vrb("mmap %u`",inc);
    minpool = osmmap(inc,ub1,1);
    minpos = 0;
    mintop = inc;
    mintot += inc;
    addsum(inc);
    if (inc == minchk && (mincnt & 3) == 3) inc <<= 1;
    minchk = inc;
    mincnt++;
  }
  p = (ub1 *)minpool + minpos;

  VALGRIND_MALLOCLIKE_BLOCK(p,n,redzone,fill <= 0xff);

  if (fill <= 0xff) { genmsg2(fln,Vrb,"set %x for %s",fill,dsc); memset(p,fill,n); }
  minpos += n + redzone;
#endif
  return p;
}

// helper for bookkeeping data
static struct ainfo *getai(const void *p,bool ismmap,bool add)
{
  ub8 hsh8 = ptrhash((ub8)p);
  ub4 hsh2,hsh,mask,bit;
  ub4 pos,pos0;
  struct ainfo *ai,*aibas;

  if (ismmap) {
    aibas = aitab2;
    bit = Aihshbit2;
    mask = (1U << Aihshbit2)-1;
  } else {
    aibas = aitab1;
    bit = Aihshbit1;
    mask = (1U << Aihshbit1)-1;
  }

  hsh = hsh8 & mask;
  ai = aibas + hsh;
  if (ai->ptr == p) {
    return ai;
  }

  hsh2 = (hsh8 >> bit) & mask;
  pos = (hsh + hsh2) & mask;
  ai = aibas + pos;
  if (ai->ptr == p) return ai;
  do {
    pos0 = pos;
    pos = (pos + 1) & mask;
    ai = aibas + pos;
    if (ai->ptr == p) return ai;
    else if (ai->ptr == nil) {
      if (add == 0) return nil;
      ai->ptr = p;
      aiuses[ismmap]++;
      return ai;
    }
  } while (pos != pos0);

  if (add) info("adr info table %u size %u full: not releasing",ismmap,mask+1);
  return nil;
}

#ifdef Diralloc
void achkfree(void) {}

#else
static ub4 chkfree(bool ismmap)
{
  struct ainfo *ai,*aibas;
  ub4 a,len,n,cnt=0;
  ub4 y;
  ub2 allan;

  if (ismmap) {
    aibas = aitab2;
    len = 1U << Aihshbit2;
  } else {
    aibas = aitab1;
    len = 1U << Aihshbit1;
  }

  for (a = 0; a < len; a++) {
    ai = aibas + a;
    if (ai->ptr && ai->freeanchor == hi16) {
      allan = ai->allocanchor;
      cnt++;
      y = elsizes[allan];
      n = ai->len;
      if (n > 8192) genmsg2(flns[allan],Warn,"unfreed %clk %u`B '%s'",ismmap ? 'B' : 'b',n,descs[allan]);
    }
  }
  return cnt;
}

void achkfree(void)
{
  showcnt("unfreed block",chkfree(0));
  showcnt("unfreed Block",chkfree(1));
}
#endif

void *alloc_fln(ub4 fln,ub8 nelem,ub4 elsiz,ub2 fil,const char *desc,ub2 counter)
{
  ub1 *p;
  ub8 n8;
  ub4 align;

  align = min(elsiz,maxalign);
  genmsg2(fln,Vrb,"+alloc %lu * %u @%u %s",nelem,elsiz,align,desc);

#ifdef Diralloc
  n8 = nelem * elsiz;
  p = malloc(n8);
  memset(p,fil,n8);

#else
  ub8 x8,n,nn;
  ub4 nm,totalmb;
  struct ainfo *ai;
  ub2 allan,mod;
  bool ismmap;

  if (elsiz == 0) fatal(fln,"zero elsize for %s",desc);

  if (nelem <= mini_thres && elsiz <= mini_thres) {
    if (nelem == 0 && (fil & Mo_ok0) ) return nil;
    n = nelem * elsiz;
    if (n < mini_thres) {
      p = minalloc_fln(fln,n,nxpwr2(align,nil),fil,desc);
      return p;
    }
  }

  // check for zero
  if (nelem == 0) fatal(fln,"zero elems for %s",desc);
  else if (elsiz == hi32) fatal(fln,"4G elsize for %s",desc);
  else if (elsiz > hi16) fatal(fln,"64KB+ elsize for %s",desc);

  n8 = nelem * elsiz;
  if (n8 >= 1UL << 34) fatal(fln,"%lu``* %u` = 8GB+ for %s",nelem,elsiz,desc);
  n = n8;

  nm = n >> 20;

  if (nm >= Maxmem_mb) {
    fatal(fln,"exceeding %u MB limit by %u %s",Maxmem_mb,nm,desc);
  }
  totalmb = totalkb >> 10;
  if (totalmb + nm >= Maxmem_mb) {
    fatal(fln,"exceeding %u MB limit by %u+%u=%u MB %s",Maxmem_mb,totalmb,nm,nm + totalmb,desc);
  }

  nn = align8(fln,n,nxpwr2(align,nil),desc);
  addsum(nn);
  if (nn >= mmap_thres) {
    ismmap = 1;
    genmsg2(fln,Vrb,"Alloc %lu``B %s",nn,desc);
    p = osmmap(nn,ub1,(fil & Mnores) != 0);
    if (!p) fatal(fln,"cannot alloc %lu``B, total %u MB for %s: %m",nn,totalmb,desc);
    if (fil && fil < Mnofil) { genmsg2(fln,Info,"set %x for %s",fil,desc); memset(p,fil,nn); }
  } else {
    nn = n;
    ismmap = 0;
    genmsg2(fln,Vrb,"alloc %lu``B %s",nn,desc);
    p = malloc(nn);
    if (!p) fatal(fln,"cannot alloc %lu``B, total %u MB for %s: %m",nn,totalmb,desc);
    if (fil < Mnofil) { genmsg2(fln,Vrb,"set %x for %s",fil,desc); memset(p,fil,nn); }
    x8 = (ub8)p & pagemask;
    if (x8 <= maxalign) return p; // not freed
  }

  ai = getai(p,ismmap,1);
  if (ai == nil) return p;

  mod = fln >> 16;
  if (mod > Shsrc_mem) return p;

  allan = mod * Memdesc + counter;
  descs[allan] = desc;
  flns[allan] = fln;
  if ( (n = elsizes[allan]) && n != elsiz) fatal(fln,"elsize %lu vs %u %s",n,elsiz,desc);
  elsizes[allan] = elsiz;

  ai->len = nn;
  ai->allocanchor = allan;
  ai->freeanchor = hi16;

#endif
  return p;
}

static inline bool is_mmapped(ub8 p)
{
  p &= pagemask;

  return (p <= maxalign); // our alloc avoids this range
}

void afree_fln(ub4 fln,void *p,const char *dsc,ub2 counter)
{
  genmsg2(fln,Vrb,"free %s",dsc);

#ifdef Diralloc
  free(p);

#else
  struct ainfo *ai;
  ub4 elsiz;
  ub4 len;
  ub8 x8;
  ub2 allan,freean,anchor,mod;
  bool ismmap;

  if (dsc == nil) dsc = "(nil)";

  if (!p) { errorfln(fln,FLN,"free nil pointer for %s",dsc); return; }
  x8 = (ub8)p;
  if (x8 & minalignmask) return; // minpool

  ismmap = is_mmapped(x8);

  ai = getai(p,ismmap,0);
  if (ai) {
    freean = ai->freeanchor;
    allan = ai->allocanchor;
    elsiz = elsizes[allan];
    len   = ai->len;

    if (freean != hi16) {
      genmsg2(flns[freean],Info,"%s location of previous free",dsc);
      errorfln(fln,FLN,"%s double free of pointer %p",dsc,p);
      genmsg2(flns[allan],Info,"%s location of allocation %s",dsc,descs[allan]);
      return;
    }

    mod = fln >> 16;
    if (mod > Shsrc_mem) return;
    anchor = mod * Memdesc + counter;
    descs[anchor] = dsc;
    flns[anchor] = fln;
    elsizes[anchor] = elsiz;
    ai->freeanchor = anchor;
  } else return;

  subsum(len);
  if (ismmap) {
    osmunmap(p,len);
  } else free((void *)p);
#endif
}

void afree0_fln(ub4 fln,void *p,const char *desc,ub2 counter)
{
  if (p) afree_fln(fln,p,desc,counter);
}

void *allocset_fln(ub4 fln,struct mempart *parts,ub2 npart,ub2 fil,const char *desc,ub2 counter)
{
  ub4 len=0,len2;
  ub4 nel,siz;
  ub2 align,align0=1;
  ub2 f;
  ub2 part;
  char *bas,*p;
  bool ismmap;
  struct mempart *pp;

  for (part = 0; part < npart; part++) {
    nel = parts[part].nel;
    if (nel == 0) continue;
    siz = parts[part].siz;
    if (siz == 0) ice(fln,hi32,"part %u nil elsiz for cnt %u %s.%s",part,nel,desc,parts[part].dsc);
    align = parts[part].algn = nxpwr2(min(siz,16),nil);
    if (len == 0) {
      align0 = align;
    } else len = align8(fln,len,align,desc);
    len += nel * siz;
  }

  len2 = (len + align0) / align0;
  bas = alloc_fln(fln,len2,align0,fil,desc,counter);
  ismmap = is_mmapped((ub8)bas);

  len = 0;
  for (part = 0; part < npart; part++) {
    pp = parts + part;
    nel = pp->nel;
    if (nel == 0) continue;
    siz = pp->siz;
    align = pp->algn;
    len = align8(fln,len,align,desc);
    pp->ptr = p = bas+len;
    f = pp->fil;
    if (fil == Mnofil && f != Mnofil) {
      if (f || ismmap == 0) memset(p,f,nel * siz);
    }
    len += nel * siz;
  }
  return bas;
}

static ub4 Expinc = (1U << 20);
static ub2 Inilim = 4096;

// expandable linear block of items
ub4 blkexp_fln(ub4 fln,struct expmem *xp,ub4 cnt,ub4 typsiz)
{
  ub1 *bas = xp->bas;
  ub4 nn;
  ub4 pos = xp->pos;
  ub4 top = xp->top;
  ub4 inc = xp->inc;
  ub2 elsiz = xp->elsiz;
  ub2 align = xp->align;
  ub2 mapcnt;

  genmsg2(fln,Vrb,"blkexp %u %s",cnt,xp->desc);

  if (typsiz != elsiz) ice(fln,hi32,"blk %s: elsiz %u vs %u",xp->desc,elsiz,typsiz);
  if (elsiz == 0) ice(fln,hi32,"blk %s: zero elsiz",xp->desc);

  if (elsiz > align) {
    nn = align4(elsiz,align);
    if (nn != elsiz) ice(fln,hi32,"blk %s: unhandled align %u for size %u",xp->desc,nn,elsiz);
  }

  nn = align4(cnt * elsiz,align);
  xp->pos = nn;
  if (bas == nil) {
    nn = max(nn * elsiz,xp->ini);
    if (nn <= Inilim) {
      bas = minalloc_fln(fln,nn,align,0,"blkexp");
      xp->min = 1;
    } else {
      xp->min = 0;
      genmsg2(fln,Info,"blkexp %u`",nn);
      nn = align4(nn,inc);
      bas = osmmapfln(fln,nn,1,1);
      addsum(nn);
    }
    xp->bas = bas;
    xp->top = nn;
  } else if (pos + nn >= top) {

    nn = align4(pos + nn,inc);
    if (xp->min) {
      genmsg2(fln,Info,"blkexp +%u`B",nn);
      bas = osmmapfln(fln,nn,1,1);
      xp->min = 0;
      memcpy(bas,xp->bas,pos);
    } else {
      mapcnt = xp->mapcnt;
      if (inc < Expinc && (mapcnt & 3) == 0) { inc <<= 1; xp->inc = inc; }
      xp->mapcnt = mapcnt + 1;
      bas = osmremapfln(fln,bas,1,top,nn);
      addsum(nn - top);
    }
    xp->bas = bas;
    xp->top = nn;
  }
  return pos;
}

ub1 *blk_ptr(struct expmem *xp,ub4 itm)
{
  return xp->bas + itm;
}

void *nearblock(void *p) // todo
{
  return p;
}

#if 0
int memrdonly(void *p,ub8 len)
{
  if ((ub8)p & 4095) return 0;
  return osmemrdonly(p,len);
}
#endif

ub4 meminfo(void) { return osmeminfo(); }

void memcfg(ub4 maxvm)
{
#ifdef Asan
  infofln(FLN,"no soft VM limit in asan");
  return;
#endif

  info("setting soft VM limit to %u GB",maxvm);
//  Maxmem_mb = (maxvm == hi24 ? hi32 : maxvm * 1024);
}

void inimem(void)
{
  pagemask = ospagesize - 1;
}

void eximem(bool show)
{
  vrb("max ai use %u,%u",aiuses[0],aiuses[1]);
  if (show & globs.resusg) info("max net   mem use %lu`B",(ub8)maxkb << 10);
}
