/* tim.c - calendar date and time

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

/* Logic dealing with wall-clock time
   Conversions, formatting, timezones

   Internal standard is minutes UTC since Epoch, typ 2000
   This is kept in an unsigned 32 bit integer
   UTC offset is minutes plus 12 hours kept as an unsigned integer.
   weekdays start at monday = 0 .. sunday = 6

   in some places a 'coded decimal' unsigned integer is used for a date
 */

#include "base.h"
#include "mem.h"
#include "tim.h"

#include "fmt.h"

static ub4 msgfile = Shsrc_time;
#include "msg.h"

static ub2 daysinmon[12] =  {31,28,31,30,31,30,31,31,30,31,30,31};
static ub2 daysinmon2[12] = {31,29,31,30,31,30,31,31,30,31,30,31};
static ub2 *jultab;
static ub2 jultablen;

static cchar monnams[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

static void initime(void)
{
  ub4 yy,mm,dd,d,y;
  ub4 jd;
  ub2 *dim;

  jd = (Erayear - Epochyear) * 366;
  jultab = (ub2 *)minalloc(jd * 2,2,hi16,"time jultab");

  // create julian day to calendar dates table, supporting typically 50 years
  // mktime() is hardly useful as it refers to a fixed system TZ.
  jd = 0;
  for (yy = Epochyear; yy < Erayear; yy++) {
    if (yy % 4) dim = daysinmon;
    else dim = daysinmon2;
    y = yy - Epochyear;
    for (mm = 0; mm < 12; mm++) {
      dd = dim[mm];
      for (d = 0; d < dd; d++) {
        jultab[jd++] = d | (mm << 5) | (y << 9);
      }
    }
  }
  jultablen = jd;
}

static ub2 julday2cal(ub2 julday)
{
  if (jultablen == 0) initime();
  return julday < jultablen ? jultab[julday] : 0;
}

ub2 nixday2cal(ub4 nixday)
{
  if (nixday < Nix2Epoch) { warning("day %u before epoch %u",nixday,Epochyear); return 0; }
  return julday2cal(nixday - Nix2Epoch);
}

static char buf[64];

cchar *fmtdate(ub2 day,ub4 mins)
{
  ub4 pos;

  ub2 d,m,y;

  y = day >> 9;
  m = (day >> 5) & 0xf;
  d = day & 0x1f;

  pos = mysnprintf(buf,0,64,"%2u %.3s %u",d+1,monnams + 3 * m,y + Epochyear);
  if (mins != hi32) {
    mins %= (24 * 60);
    mysnprintf(buf,pos,64," %2u:%02u",mins / 60,mins % 60);
  }
  return buf;
}

cchar *fmtmsec(ub8 msec)
{
  ub4 ms = (ub4)(msec % 1000);
  ub4 s = (ub4)(msec / 1000);
  ub4 d = s / 86400;

  s %= 86400;
  ub4 h = s / 3600;
  ub4 m = (s / 60) % 60;
  s %= 60;

  ub2 c = nixday2cal(d);
  ub2 yr = (c >> 9) + Epochyear;
  ub2 mn = (c >> 5) & 0xf;
  ub2 dy = c & 0x1f;
  mysnprintf(buf,0,64,"%4u-%02u-%02u.%02u.%02u.%02u.%03u",yr,mn,dy,h,m,s,ms);
  return buf;
}
