/* pre.h - interface between pre and lex

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

struct prelex {
  ub4 tkcnt,tacnt,cmtcnt;

  cchar *src;
  ub4 srclen;

  ub4 slit1cnt,slit2cnt,slitncnt;
  ub4 slittop;

  ub4 nlitcnt;

  ub4 bitcnt;

  ub4 lncnt;
  ub4 modcnt;
  ub4 filcnt;
  struct filinf *files;

  ub2 incdircnt;
  const char **incdirs;
};

enum Inctyp { Inone,Isys,Iuser,Icmd };

extern int prelex(cchar *path,enum Inctyp inc,struct prelex *lsp,ub8 T0);
extern void inipre(void);
