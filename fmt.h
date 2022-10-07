/* fmt.h - printf-style string formatting

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

extern ub4 mysnprintf(char *dst,ub4 pos,ub4 len,const char *fmt,...) __attribute__ ((format (printf,4,5)));

#ifdef va_start
 extern ub4 myvsnprint(char *dst,ub4 pos,ub4 len,const char *fmt,va_list ap);
#endif

extern int snprintf(char *dst,ub8 len,cchar *fmt,...) __attribute__ ((format (printf,3,4)));

#define fmtstring(dst,fmt,...) mysnprintf((dst),0,sizeof(dst),(fmt),__VA_ARGS__)

extern char *utoa(char *end,ub4 x);
extern char *itoa(char *end,sb4 x);
extern char *ultoa(char *end,unsigned long x);
