/* net.c - networking specifics

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


#include <sys/socket.h>
//#include <netinet/in.h>
#include <arpa/inet.h>

#include "base.h"

#include "net.h"

static ub4 msgfile = Shsrc_net;
#include "msg.h"

int netsocket(int v6)
{
  int dom = v6 ? AF_INET6 : AF_INET;

  int fd = socket(dom,SOCK_DGRAM,0);
  return fd;
}

int netsend(int fd,const ub1 *daa,unsigned short port,const void *buf,unsigned short len)
{
  struct sockaddr_in sa;
  in_addr_t da = (daa[0] << 24) | (daa[1] << 16) | (daa[2] << 8) | daa[3];

  if (da == INADDR_NONE) return 1;

  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = da;
  sa.sin_port = htons(port);

  int nw = sendto(fd,buf,len,0,(struct sockaddr *)&sa,sizeof(sa));
  return nw;
}

int netbind(int fd,unsigned short port)
{
  int rv;
  struct sockaddr_in sa;

  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = INADDR_ANY;
  sa.sin_port = htons(port);
  rv = bind(fd,(struct sockaddr *)&sa,sizeof(sa));
  return rv;
}

int netrcv(int fd,void *buf,unsigned short len)
{
  struct sockaddr_in sa;
  socklen_t alen = sizeof(sa);

  int nr = recvfrom(fd,buf,len,0,(struct sockaddr *)&sa,&alen);

  info("received %d bytes from %x:%u",nr,ntohl(sa.sin_addr.s_addr),ntohs(sa.sin_port));
  return nr;
}
