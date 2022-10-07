/* tim.h - calendar date and time

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

#define Epochyear 2022

// 1-1-2022 = sat
#define Epochwday 5

// time horizon
#define Erayear 2030

#define Nix2Epoch (1640995200 / 86400)

// c11 langage only
#if defined  __STDC_VERSION__ && __STDC_VERSION__ >= 201101
  _Static_assert(Epochyear > 1969,"time before 1970 not handled");
  _Static_assert(Epochyear < 2100,"time after  2100 not handled");
  _Static_assert(Epochyear < Erayear,"must have a time span");
  _Static_assert(Erayear - Epochyear < 100,"time span too large");
  _Static_assert(Erayear > Epochyear,"time span too small");
#endif

extern ub2 nixday2cal(ub4 nixday);
extern cchar *fmtdate(ub2 x,ub4 mins);
extern cchar *fmtmsec(ub8 msec);
