# pre.lex - pre-lexical analysis for Lua language

#  This file is part of Luanova, a fresh implementation of Lua.

#  Copyright © 2022 Joris van der Geer.

#  Luanova is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Affero General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.

#  Luanova is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#  GNU General Public License for more details.

#  You should have received a copy of the GNU Affero General Public License
#  along with this program, typically in the file License.txt
#  If not, see http://www.gnu.org/licenses.

version 0.1.0
author joris
language lua
requires genlex 1.0

set
  af a-qs-zA-QS-Z\Z_
  nm 1-9
  n0 0
  ht \09
  vt \0c
  cr \0d
  nl \0a
  hs #
  sq '
  dq "
  rr r
  bo [{(
  bc ]})
  dt .
  op @%&^|*/~<>!+-
  pc ,?;:=
  EOF \00
.
  +an a-zA-Z\Z\z0-9_
  +nx 0-9._
  +xn 0-9A-Fa-f_
  +eE eE
  +xX xX
  -ws \20

action

# ---------------------
# check import
# ---------------------
chkimp
  len = n - N;
  if (len == 8 && memcmp(sp+N,"requires",8) == 0) {
    isreq = 1;
  } else { isreq = 0; tacnt++; }

# ---------------------
# handle idents
# ---------------------

id
  len = n - N;
  if (len > 2) {
    hc = hashstr(sp + N,len,0);
    idcnt++;
    exp_first0(hc,idsketchs);
  }

# ---------------------
# handle slit
# ---------------------
doslit
  len = n - N;
  if (isreq) {
    isreq=0;
    addmod(spos,N,len,1);
    modcnt++;
  }
  switch (len) {
  case 1: slit1cnt++; break;
  case 2: slit2cnt++; break;
  default: slitncnt++;
  }
  slitpos += len;

# ---------------------
# handle braces [{(
# ---------------------
dobo
  if (bolvl >= Depth) lxerror(l,0,"root",nil,bolvl,"exceeding nesting depth");
  bolvls[bolvl] = l;
  bolvlc[bolvl++] = c;
  tkcnt++;

dobc
  if (bolvl == 0) {
    lxinfo(bclvls[0],0,"final close here");
    lxerror(l,0,"root",nil,c,"unbalanced");
  }
  bolvl--;
  if (bolvlc[bolvl] != c) {
    lxinfo(bolvls[bolvl],0,"opened here");
    lxerror(l,0,"root",nil,c,"unmatched");
  }
  bclvls[bolvl] = l;
  tkcnt++;

# ---------------------
table
# ---------------------

# ---------------------
# initial state
# ---------------------
root.N

# pat nxstate token action

# whitespace and newlines
  `ws
  ht
  cr
  vt
  nl . . .l++; nlcol = n;

# line comments
  hs cmt

# id / kwd
  af id
  rr xid

  n0 n0
  nm nm

  bo . . dobo
  bc . . dobc

# string literal
  sq slits0
  dq slitd0

  dt dot
  op . . .tacnt++;
  pc . . .tkcnt++;

  EOF EOF . .if (bolvl) { lxinfo(l,0,"opened here"); lxerror(l,0,$S,$P,bolvlc[0],"unmatched"); }

#  ot . . .tkcnt++;

# --- end root ---

# ---------------------
# line comment
# ---------------------
cmt
  .nl root . .cmtcnt++;
  .EOF EOF . .if (bolvl) { lxinfo(l,0,"opened here"); lxerror(l,0,$S,$P,bolvlc[0],"unmatched"); }
  ot

dot
  nm nm
  eE fxp0
  ot -root . .tkcnt++;

# ---------------------
# identifier or keyword.
# ---------------------
id
  an id1
  ot -root . .tacnt++; # id1

id1
  an id2
  ot -root . .tacnt++; # id2

id2
  an
  ot -root . .tacnt++;

xid
  af
  ws root . chkimp
  ot -root . `isreq=0; tacnt++;` id

# ---------------------
# num literal.
# ---------------------
n0
  xX xnm
  nx nm1
  ot -root . .nlit1cnt++;

nm1
  nx nm
  eE fxp0
  ot -root . .nlit1cnt++;

nm
  nx
  eE fxp0
  ot -root . .nlitcnt++; if (n - N < 4) acnt++; else bitcnt++;

xnm 0x
 xn
 ot -root . .nlitcnt++; if (n - N < 4) acnt++; else bitcnt++;

fxp0
  pm fxp
  nm fxp
  ot -root . .nlitcnt++;

fxp
  nm
  ot -root . .nlitcnt++;

# ---------------------
# string literal start
# ---------------------
slits0
  .sq root . .tacnt++; # empty short slit ''  no slitctl ?
  .nl  -slits
  .EOF -slits
  \ -slits
  ot slits . .N=n; L=l; Nlcol=nlcol;

slitd0
  .dq root . .tacnt++;
  .nl  -slitd
  .EOF -slitd
  \ -slitd
  ot slitd . .N=n; L=l; Nlcol=nlcol;

# ---------------------
# string literal
# ---------------------
slits string literal
  .sq slit9
  \sq
  \nl   . . .lxerror(l,n-nlcol,$S,$P,hi16,"escaped newline in slit"); $!
  \
  .EOF  . . .lxinfo(L,N-Nlcol,"slit started here"); lxerror(l,n-nlcol,$S,$P,hi16,"missing end quote (')"); $!
  .nl   . . .lxerror(l,n-nlcol,$S,$P,hi16,"newline in slit"); $!
  ot

slitd string literal
  .dq slit9
  \dq
  \nl   . . .lxerror(l,n-nlcol,$S,$P,hi16,"escaped newline in slit"); $!
  \
  .EOF  . . .lxinfo(L,N-Nlcol,"slit started here"); lxerror(l,n-nlcol,$S,$P,hi16,"missing end quote (\")"); $!
  .nl   . . .lxerror(l,n-nlcol,$S,$P,hi16,"newline in slit"); $!
  ot

# ---------------------
# string literal end
# ---------------------
slit9
  ot -root . doslit
