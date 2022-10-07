/* map.h - maps aka hash tables

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

struct map { // 32
  ub4 *tab;
  ub8 *items;  // hash.32 keyndx.32. curid = count = items[0]
  ub1 *keys;   // expand

  ub4 keypos;

  ub1 siz;     // key.2 itm.2 tab.2
  ub1 tbit;
  ub1 ibit;
  ub1 kbit;
};

extern void mkmap(struct map *m,ub4 estcnt,ub4 estkeylen);
extern void finmap(struct map *m);

// get or if none insert
extern ub4 mapgetadd(struct map *m,const ub1 *nam,ub2 len,ub4 hc);

extern ub1 *getkey(struct map *m,ub4 x);

extern void inimap(void);
