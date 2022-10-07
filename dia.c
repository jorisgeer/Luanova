/* dia.c - diag

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

#include "base.h"

static ub4 msgfile = Shsrc_dia;
#include "msg.h"

#include "dia.h"

static struct diamod *mods[Dm_count] = {
  [Dm_lex] = &dia_lex
};

static bool streq(cchar *p,cchar *q)
{
  if (!p || !q || (*p | *q) == 0) return 0; // do not accept both empty

  while (*p) {
    if (*p != *q) return 0;
    p++; q++;
  }
  return (*q == 0);
}

static bool memeq(cchar *p,cchar *q,ub2 n)
{
  ub4 i = 0;
  while (i < n && p[i] == q[i]) i++;
  return i == n;
}

#if 0
void diaini(enum Diamod mod,cchar ** names,ub2 *lvls,ub2 tagcount)
{
  struct diamod *mp = mods + mod;

  mp->names = names;
  mp->lvls = lvls;
  mp->cnt = tagcount;
}
#endif

static ub2 findtag(struct diamod *m,cchar *t)
{
  cchar *p,**pp = m->names;
  char c;
  ub2 n = 0,nt = m->cnt;

  c = *t;
  while (n < nt && pp[n][0] < c) n++;
  if (n == nt) return hi16;
  p = pp[n];
  if (*p > c) return hi16;
  while (n < nt && *p == c) {
    if (streq(p,t)) return n;
    p = pp[++n];
  }
  return hi16;
}

int diaset(enum Msglvl lvl,cchar *list)
{
  cchar *t,*q,*p = list;
  ub2 ndx,n;
  struct diamod *mp;
  enum Diamod mod;

  if (!p || !*p) return 0;

  while (*p) {
    q = p;
    while (*q && *q != ',' && *q != '.') q++;
    n = q - p;
    if (*q == 0) { error("Expected module.tag, found %.*s",n,p); return 1; }
    else if (*q == ',') { error("Missing module or tag for %.*s",n,p); return 1; }
    if (n != 3) { error("Unknown module %.*s",n,p); return 1; }
    if (memeq(p,"lex",3)) mod = Dm_lex;
    else if (memeq(p,"syn",3)) mod = Dm_syn;
    else { error("Unknown module %.*s",n,p); return 1; }

    q++; t = q;
    while (*q && *q != ',') q++;
    n = q - p;
    if (n == 0) { error("missing diag tag for %.3s",p); return 1; }

    mp = mods[mod];
    if (mp == nil || mp->names == nil) fatal(0,0,"module %.3s not inited",p);
    ndx = findtag(mp,t);
    if (ndx == hi16) { error("Unknown diag tag %.*s",n,t); return 1; }
    else if (ndx >= mp->cnt) fatal(0,0,"invalid tag %u for %.*s",ndx,n,t);
    mp->lvls[ndx] = lvl;
    if (*q == 0) break;
    else p = q + 1;
  }
  return 0;
}
