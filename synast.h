/*  synast.h - common defines between parser and ast builder

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

struct synast {
  cchar *name;
  ub8 *args;
  ub8 *vals;
  ub4 *nhs;
  ub4 *fps;
  ub2 *nlnos;
  ub8 *repool;
  ub4 idcnt,uidcnt;

  const ub1 *slitpool;
  ub4 slithilen;

  ub4 aidcnt,argcnt,valcnt;
  ub4 repcnt;
  ub4 nscid;
  ub4 startnd;
  ub4 ndcnts[Acount+1];
  ub4 rep2cnts[Acount+1];
};

#define Nodarg 5
#define Nodval 8
#define Argbit 24
#define Argmsk hi24

extern void *mkast(struct synast *sa);
