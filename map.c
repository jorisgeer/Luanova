/* map.c - maps aka hash tables

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

#include <string.h>

#include "base.h"

#include "os.h"

#include "mem.h"

static ub4 msgfile = Shsrc_map;
#include "msg.h"

#include "map.h"

//#include "hash.h"

/*
  string key
  int key
  compound key
  sequence number as value - symtab. hash supplied
  integer values:  hash supplied
  no values - no hash either
 */

enum Size { Sznil,Szmin,Szmal,Szmap };
static const ub4 Minalloc = 16;
static const ub4 Malloc = 4096 * 16;

static void *expand(void *op,ub4 on,ub4 nn,ub2 algn,ub1 *ps)
{
  enum Size s = *ps;
  ub1 *np;

  switch(s) {
  case Sznil: case Szmin:
    if (nn < Minalloc) {
      np = minalloc(nn,algn,Mnofil,"");
      s = Szmin;
    } else if (nn < Malloc) {
      np = myalloc(nn);
      s = Szmal;
    } else {
      np = osmmapfln(FLN,nn,1,1);
      s = Szmap;
    }
    if (s == Szmin) memcpy(np,op,nn - on);
    break;
  case Szmal:
    if (nn < Malloc) {
      np = remalloc(op,nn);
    } else {
      np = osmmapfln(FLN,nn,1,1);
      memcpy(np,op,nn - on);
      s = Szmap;
    }
    break;
  default:
    np = osmremapfln(FLN,op,1,on,nn);
  }
  *ps = s;

  return np;
}

void mkmap(struct map *m,ub4 estcnt,ub4 estkeylen)
{
  ub4 hlen,itmtop;
  ub4 *tab;
  ub1 kbit,bit = 0;
  ub1 tsiz,ksiz,isiz;

  memset(m,0,sizeof(*m));

  if (estcnt == 0) return;

  hlen = 1;
  while (hlen < estcnt) { hlen <<= 1; bit++; }
  itmtop = hlen;
  m->ibit = bit;

  hlen <<= 2;
  bit += 2;

  m->tbit = bit;

  tab = expand(nil,0,hlen,4,&tsiz);
  if (tsiz == Szmal) memset(tab,0,hlen);

  estkeylen = nxpwr2(estkeylen,&kbit);
  m->keys = expand(nil,0,estkeylen,1,&ksiz);

  m->items = expand(nil,0,itmtop,8,&isiz);
  m->siz = (ksiz << 4) | (isiz << 2) | tsiz;
}

void finmap(struct map *m)
{
  ub4 *tab = m->tab;

  if (tab == nil) return;
  switch (m->siz & 3) {
    case Sznil: case Szmin: return;
    case Szmal: mfree(tab); break;
    default: osmunmap(tab,1U << m->tbit);
  }
}

static ub4 mapadd(struct map *m,const ub1 *nam,ub2 len,ub4 v)
{
  ub4 *ntab,*tab = m->tab;
  ub4 x;
  ub4 w;
  ub4 np,id;
  ub1 ibit = m->ibit;
  ub4 nlen,ilen = 1U << ibit;
  ub8 *items = m->items;
  ub1 kbit = m->kbit;
  ub4 klen = 1U << kbit;
  ub1 *keys = m->keys;
  ub1 tbit = m->tbit;
  ub4 tlen = 1U << tbit;
  ub4 hsh,msk;
  ub1 tsiz,ksiz,isiz,siz = m->siz;

  id = items ? (ub4)items[0] : 1;

  if (id >= ilen) { // resize items
    nlen = ilen * 2;
    isiz = (siz >> 2) & 3;
    info("hsh %p resize items to %u",m,nlen);
    items = expand(items,ilen,nlen,8,&isiz);
    m->items = items;
  }
  items[0] = id + 1;

  if (id * 2 > tlen) {
    tsiz = siz & 3;
    ntab = expand(tab,tlen,tlen * 2,4,&tsiz);
    msk = tlen * 2 - 1;
    for (w = 0; w < tlen; w++) {
      x = tab[w];
      if (x == 0) continue;
      hsh = items[x] >> 32;
      if (hsh & tbit) { // new bit set, move
        ntab[w << 1] = x;
        ntab[w] = 0;
      }
    }
  }

  tab[v] = id;
  np = m->keypos;

  if (np + len + 1 > klen) {
    ksiz = (siz >> 4) & 3;
    info("hsh %p resize keys %u %u",m,np+len,klen * 2);
    keys = expand(keys,klen,klen * 2,1,&ksiz);
    m->keys = keys;
  }
  memcpy(keys+np,nam,len);
  keys[np+len] = 0;

  items[id] = np;
  info("add id %u nid %u",id,np);
  m->keypos = np + len + 1;

  return id; // new
}

static ub4 check(struct map *m,const ub1 *nam,ub2 len,ub4 x)
{
  ub4 np;
  ub1 *keys = m->keys;

  np = m->items[x] & hi32;
  if (keys[np+len] == 0 && memcmp(keys+np,nam,len) == 0) return x; // existing: common
  else {
    return hi32;
  }
}

// get or if none insert
ub4 mapgetadd(struct map *m,const ub1 *nam,ub2 len,ub4 hc)
{
  ub4 v;
  ub4 x;
  ub4 hc2;
  ub1 tbit = m->tbit;
  ub4 tlen = 1U << tbit;
  ub4 msk = tlen-1;
  ub4 *tab = m->tab;

  // info("add %.*s",len,nam);
  v = hc & msk;
  x = tab[v];
  if (x == 0) return mapadd(m,nam,len,v);
  else if (check(m,nam,len,x)) return x;
  else { // probe once with second hash
    hc2 = hc >> tbit;
    v = (v + hc2) & msk;
    x = tab[v];
    if (x == 0) return mapadd(m,nam,len,v);
    else if (check(m,nam,len,x)) return x;
    else {
      do { // linear
        v = (v + 1) & msk;
        x = tab[v];
        if (x == 0) return mapadd(m,nam,len,v);
        else if (check(m,nam,len,x)) return x;
      } while (1);
    }
  }
}

ub1 *getkey(struct map *m,ub4 x)
{
  ub4 np = m->items[x] & hi32;
  return m->keys + np;
}

void inimap(void)
{
  lastcnt
}
