/* syn.h - front end definitions for parser

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

// max number of node args
#define Nodarg 5

// max length of production in symbols
#define Slen 12

#define Crepshift 3

#define Laset 4

#define Skip 8

#define Repcnt hi24

/* symbol controls
 * b0-2 node args 0=none
 * b3-5 group skips 0=none 7=+rep 6=*
 * b6-7 id ctl
 */
enum Packed8 Ctl {

  Crep11=0, // plain

  // arguments
  Carg1=1,Carg2,Carg3,Carg4,Cargmask=7,

  // repetition and grouping
  Crep01=0x8,Crep02=0x10,Crep03=0x18,Crep04=0x20,Crep05=0x28,
  Crep0n=0x30,Creplp=0x38,

  Crepmask=0x38,

  // typedef / id
  Ciddef=0x40,Cidref=0x80,Cidmask=0xc0
};

#ifdef __clang__
 #pragma clang diagnostic warning "-Wduplicate-enum"
#endif

enum Satrs { Sa_none,Sa_len = 0xf,Sa_si = 0x30,Sa_s0=0x40,Sa_rep=0x80,Sa_arg=0x7ff,Sa_nod=0x800 };

struct sentry {
  ub1 syms[Slen];
  ub1 ctls[Slen];

// gen only
#ifdef Gensyn
  ub1 len; // #syms
  ub1 nve;
  ub1 ve0;
  ub1 nt0;    // enum Nterm current rule
  ub1 s0;
  ub1 alt;
  ub4 la;
  ub2 lno;
  char src[44];
#endif
};

struct seinfo { // for diags
  ub2 lno;
  ub1 alt;
  ub1 s0;
  cchar *src;
};
