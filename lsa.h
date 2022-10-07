/* lsa.h - common defines between lex, syn and ast

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

// litctl
#define Lxabit 26
#define Lxamsk 0x3ffffffU

// int lit radix. 0 = dec
#define Ilitbin 1
#define Ilitoct 2
#define Ilithex 3

// slit
#define Slitfbit 31
#define Slitlbit 30
#define Slitqbit 29

#define Slitctl_r 1
#define Slitctl_f 2
#define Slitctl_b 4

extern const ub1 *idnam(ub4 id);
extern ub4 slitstr(ub4 id,ub4 *plen);
