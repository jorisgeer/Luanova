/* bug.c - bug reporting

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

#include "os.h"

#include "net.h"

#define FLN __LINE__
#include "fmt.h"

void bugreport(cchar *rep,ub2 rlen,cchar *tag)
{
  ub2 blen = 2048;
  char buf[2048];
  ub2 len;
  int fd = netsocket(0);

  len = mysnprintf(buf,0,blen,"%s\n%u.%u.%u\n%.*s",
    tag,
    Version_maj,Version_min,Version_pat,
    rlen,rep);

  netsend(fd,(const ub1 *)"\x7f\0\0\1",8000,buf,min(len,1024));

  len = mysnprintf(buf,0,blen,"Bug report tagged '%s' sent\n",tag);
  oswrite(2,buf,len,nil);
}
