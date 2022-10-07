/* math.c - math utilities

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

/* math utilities */

#include "base.h"
#include "math.h"

// wikipedia xorshift
static ub8 xorshift64star(void)
{
  static ub8 x = 0x05a3ae52de3bbf0aULL;

  x ^= x >> 12; // a
  x ^= x << 25; // b
  x ^= x >> 27; // c
  return (x * 2685821657736338717ULL);
}

static ub8 rndstate[ 16 ];

static ub4 xorshift1024star(void)
{
  static int p;

  ub8 s0 = rndstate[p];
  ub8 s1 = rndstate[p = ( p + 1 ) & 15];
  s1 ^= s1 << 31; // a
  s1 ^= s1 >> 11; // b
  s0 ^= s0 >> 30; // c
  rndstate[p] = s0 ^ s1;

  return (ub4)(rndstate[p] * 1181783497276652981ULL);
}

// static ub4 rndmask(ub4 mask) { return (ub4)xorshift1024star() & mask; }

ub4 rnd(ub4 range)
{
  ub4 r;
  if (range == 0) r = 1;
  else r = (ub4)xorshift1024star();
  if (range && range != hi32) r %= range;
  return r;
}

double frnd(ub4 range)
{
  double x;

  x = rnd(range);
  return x;
}

int inimath(void)
{
  ub8 x;

  for (x = 0; x < 16; x++) rndstate[x] = xorshift64star();

  return 0;
}
