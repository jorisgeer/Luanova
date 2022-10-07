# lua.lex - lexical analysis for Lua language

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

eof so

token
# group 0 - storable term with value
  id
  nlit
  slit

# group 1 - storable term
  op 1

# group 2 - (aug)assign
  aas 2

# group 3 - scope
  co 3 {
  cc 3 }

# group 4
  sepa 4 ;
  comma 4 ,

  ro 4 (
  rc 4 )
  so 4 [
  sc 4 ]

  ao 4 <
  ac 4 >

  das 4 =
  colon 4 :
  dcol 4 ::
  dot 4 .
  ell 4 ...

set
  uu _
  af a-zA-Z\Z
  n0 0
  nm 123456789
  ht \09
  vt \0c
  cr \0d
  nl \0a
  hs #
  qq '"
.
  co {
  ro (
  so [
  cc }
  rc )
  sc ]
  ao <
  ac >
.
  cl :
  sm ;
  ca ,
  dt .
  eq =
  pm +-
  o1 @%&^|*/~
  o2 !
  EOF \00
.
 +an _a-zA-Z\Z\z0-9
 +nu 0-9
 +bB bB
 +eE eE
 +pP pP
 +xX xX
 +hx abcdefABCDEF
 -ws \20

keyword
#  and -> && op
  do
  else
  elseif
  end
  false
  for
  function
  goto
  if
  in
  local
  nil
  not
#  or -> || op
  repeat
  return
  then
  true
  until
  while
  break|continue=ctlxfer

`Bbuiltin
  _ENV
  _G
  assert
  collectgarbage
  dofile
  error
  getmetatable
  ipairs
  load
  loadfile
  next
  pairs
  pcall
  print
  rawequalk
  rawget
  rawlen
  rawset
  select
  setmetatable
  tonumber
  tostring
  type
  _VERSION
  warn
  xpcall
.
  coroutine
  close
  create
  isyieldable
  resume
  running
  status
  wrap
  yield
.
  require
  package
  config
  cpath
  loaded
  loadlib
  path
  preload
  searchers
  searchpath
.
  string
  byte
  char
  dump
  find
  format
  gmatch
  gsub
  len
  lower
  match
  pack
  packsize
  rep
  reverse
  sub
  unpack
  upper
.
  utf8
  charpattern
  codes
  codepoint
  offset
.
  table
  concat
  insert
  move
  remove
  sort
.
  math
  abs
  acos
  asin
  atan
  ceil
  cos
  deg
  exp
  flor
  fmod
  huge
  log
  max
  maxinteger
  min
  mininteger
  modf
  pi
  rad
  random
  randomseed
  sin
  sqrt
  tan
  tointeger
  ult
.
  io
  flush
  input
  lines
  open
  output
  popen
  read
  tmpfile
  write
.
  file
  seek
  setvbuf
.
  os
  clock
  date
  difftime
  execute
  exit
  getenv
  rename
  setlocale
  time
  tmpname
.
  debug
  gethook
  getinfo
  getlocal
  getregistry
  getupvalue
  getuservale
  sethook
  setlocal
  setupvalue
  setuservalue
  traceback
  upvalueid
  upvaluejoin

`Ddunder
  add
  sub
  mul
  div
  mod
  pow
  unm
  idiv
  band
  bor
  bxor
  bnot
  shl
  shr
  concat
  len
  eq
  lt
  le
  index
  newindex
  call
  gc
  close
  mode
  name

action

# ----------------------
# string literal
# ----------------------
slit
  // vrb(" add slit pos %u len %2u typ %u.%u '%s'",dn1,len,sla,R0,chprintn(sp+N-1,min(len,512),nil,sla));
  len = slitx - slitpos;
  switch(len) {
  case 1: atr = slitpool[slitpos]; ctl |= 1; break;
  case 2: atr = (slitpool[slitpos] << 8) | slitpool[slitpos]; ctl |= 2; break;
  default: id = slitgetadd(n,slitpool,slitx,ctl);
           if (id <= hi16) { atr = id; ctl |= Las_v2; }
           else bits[bn++] = id;
           ctl |= 3;
  }
  R0=ctl=0;
  atrs[an++] = atr;
  ctls[cn++] = ctl;

# ----------------------
# \xx in string literal
# ----------------------
doesc
  c = sp[n++];
  if (c < 0x80) x1 = esctab[c];
  else x1 = 0;
  switch(x1) {
  case Esc_nl: lntab[l++] = n; nlcol=n; break;
.
  case Esc_o:
    x1 = c - '0';
    c = sp[n];
    if (c >= '0' && c <= '7') { n++; x1 = (x1 << 3) + (c - '0'); } else { slitpool[slitx++] = x1; break; }
    c = sp[n];
    if (c >= '0' && c <= '7') { n++; x1 = (x1 << 3) + (c - '0'); }
    slitpool[slitx++] = x1;
    break;
.
  case Esc_x: n=doescx(sp,n,slitpool,&slitx,slitctl & Slitctl_b);
              break;
.
  case Esc_u:
  case Esc_U: n=doescu(sp,n,slitpool,&slitx,x1); break;
  case Esc_N: n=doescn(sp,n,slitpool,&slitx);    break;
.
  case Esc_inv:
    // sinfo(n,"'\\%s'",chprint(c));
    // lxwarn(l,0,`L,c,"unrecognised escape sequence");
    slitpool[slitx] = '\\';
    slitpool[slitx+1] = c;
    slitx += 2;
    break;
.
  default:    slitpool[slitx++] = x1;
  }

# ----------------------
# ident or kwd
# ----------------------

# 2 char
id2
  // info("add id.2  %s%s",chprint(prvc1),chprint(prvc2));
  if (prvc1 == 'o' && prvc2 == 'r') {
    tk = Top; atrs[an++] = '|';
  }
  else if ( (kw = lookupkw2(prvc1,prvc2)) < t99_count) tk = kw;
  else if ( (blt = lookupblt2(prvc1,prvc2)) < B99_count) { tk = id; atrs[an++] = blt | La_idblt; bltcnt++; }
  else {
    id2cnt++;
    if (prvc1 < id2loch1) id2loch1 = prvc1;
    else if (prvc1 > id2hich1) id2hich1 = prvc1;
    tk = Tid;
    an++;
  }

id2u
  id2cnt++;
  // info("add id.2  %s%s",chprint(prvc1),chprint(prvc2));
  if (prvc1 < id2loch1) id2loch1 = prvc1;
   else if (prvc1 > id2hich1) id2hich1 = prvc1;
  tk = Tid;
  an++;

# 3+ char
id
  len = n - N;
  len2 = len;
  if (len == 3 && sp[N] == 'a' && sp[N] == 'n' && sp[N] == 'd') {
    tk = Top;
    atrs[an++] = '&';
  } else {
    hc = hashstr(sp+N,len2,Hshseed);
    tk = Tid;
    kw = lookupkw(sp+N,len2,hc);
    if (kw < t99_count) {
      atr = kw; if (kw < (enum token)T99_mrg) tk = kw; else tk = kwhshmap[kw];
    } else {
      tk = Tid;
      blt = lookupblt(sp+N,len2,hc);
      if (blt < B99_count) { atr = blt | La_idblt; bltcnt++; }
      else {
        x4 = mapgetadd(&idtab,sp+N,len2,hc);
        if (x4 < La_idprv) atr = x4;
        else {
          bits[bn++] = x4 | ((ub8)dn << 32); atr = La_id4;
       }
     }
   }
  } // id

# __* = dunder
`D dunder
  idcnt++;
  len = n - N;
  // info("add id.%-2u __%.*s",len,len,chprintn(sp+N,len));
  len2 = len - 2;
  if (len2 == 2) dun = lookupdun2(sp[N],sp[N+1]);
  else dun = lookupdun(sp+N,len2);
  if (dun < D99_count) atrs[dn] = dun | La_iddun;
  else {
    x4 = mapgetadd(&idtab,sp+N,len2,hc);
    if (x4 < La_idprv) atr = x4;
    else {
      bits[bn++] = x4 | ((ub8)dn << 32); atr = La_id4;
    }
  }

`d dunder
  an++;

# __[a-z][a-z] = dunder
dunder2
  id2cnt++;
  an++;
  // info("add id.%-2u __%.*s",len,len,chprintn(sp+N,len));

# ----------------------
# int literal
# ----------------------
ilit
  if (i8 < 64) ctls[cn++] = (ub1)i8;
  else if (i8 <= hi16) { ctls[cn++] = (ub1)i8; }
  else { bits[bn++] = i8; }

# ----------------------
# newline: maintain line table
# ----------------------
donl
  lntab[l] = n;
  // sinfo(n,"ln %u n %u",l,n);
  l++;
  nlcol = n;

Token
  dfp0 = N - prvN;
  dfp1 = n - prvn;
  prvN = N;
  prvn = n;
  if (dfp0 > 1) {
    tk |= 0x80;
    if (dfp0 < 0x80) dfp0s[fn0++] = dfp0;
    else { dfp0s[fn0++] = dfp0 & 0x80; dfp0s[fn0++] = dfp0 >> 7; dfp0s[fn0++] = dfp0 >> 15; }
  }
  if (dfp1 > 1) {
    tk |= 0x40;
    if (dfp1 < 0x80) dfp1s[fn1++] = dfp1;
    else { dfp1s[fn1++] = dfp1 & 0x80; dfp1s[fn1++] = dfp1 >> 7; dfp1s[fn1++] = dfp1 >> 15; }
  }
  tks[dn] = tk;

# ---------------------
table
# ---------------------

# state lead pat

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
  nl . . donl

# line comments
  hs cmt0

# plus min
  pm pm . .atrs[an++] = c;

# int literals
  0 znlit0
  nm nlit1 . .ipart4 = c - '0';

# id or kwd
  _ u1

# id / kwd /slit pfx
  af id1 . .prvc1 = c;

# string literal - no prefix
  qq slit0 . .Q=c;

# brackets
  { . co
  } . cc

  [ . so
  ] . sc

  ( . ro
  ) . rc

  < ao1
  > ac1

  =  eq1

# punctuators
  : col1
  ; . sepa
  , . comma
  .. dot

# operators @ % & ^ | * / ~
  o1 op11 . .atrs[an++] = c;

# !
  o2 op21 . .atrs[an++] = c;

  EOF EOF
#  $$ EOF

# ---------------------
eq1
  = root op .atrs[an++] = '=';
  ot -root das

# ---------------------
col1
  : root dcol
  ot -root colon

# ---------------------
dot
 ... root ell
 .. root op
 0 flitf0
 nm flitf
 ot -root dot

# ---------------------
# operators / augassign
# ---------------------
ao1
  = root aas
  <= root aas
  < root op
  ot -root ao

ac1
  = root aas
  >= root aas
  > root op
  ot -root ac

op11
  Q op12
  = root aas
  ot -root op

op12
  = root aas
  ot -root op

op21
  = root op
  Q op22
  ot -root op

op22
  = root aas
  ot -root op

# ---------------------
# line comment
# ---------------------
cmt0
  ot -cmt . .cmt0 = N;

cmt
  .nl root . .cmtcnt++;\
             donl
  ot

# ---------------------
u1
  _ dun0 . .N=n;
  an id2
  ot -root id .id1cnt++; atrs[an++] = id1getadd('_') | La_id1;

dun0
  an dun1
  ot -root . `prvc1 = prvc2 = '_';` id2u

dun1
  an dun
  ot -root . dunder2

dun
  an
  ot -root . dunder

# ---------------------
# identifier or keyword. lookup in action
# ---------------------
id1
  an id2
  ot -root id .id1cnt++; atrs[an++] = id1getadd(prvc1) | La_id1;

id2
  an id
  ot -root id `prvc1 = sp[n-2]; prvc2 = sp[n-1];` id2

id
  an
  ot -root . id

# --------------
# sign, operator. Process tentatively for nlits
# --------------
pm
  pm
  =   root aas
  ot -root op

# ---------------------
# int lit 0...
# ---------------------
znlit0
  bB ilitb0
  xX ilitx0
  _
  0 nlit0  # consume leading zeroes
  nm nlit1 . .ipart4 = c - '0'; # decimal with leading zero
  .. flitf0
  eE nlite0  # 0e
  j   root nlit .ctls[cn++] = Lan_im; # 0i
  ot -root nlit .ctls[cn++] = 0; # 0

nlit0
  _
  0
  nm nlit1 . .ipart4 = c - '0'; # decimal with leading zero
  .. flitf0
  eE nlite0  # 0e
  j   root nlit .ctls[cn++] = Lan_im; # 0i
  ot -root nlit .ctls[cn++] = 0; # 0

# ---------------------
# int literal
# ---------------------
nlit1
  0 nlit . .ipart4 *= 10;
  nm nlit . .ipart4 = ipart4 * 10 + (c - '0');
  .. flitf0
  eE flitxs
  _
  j   root nlit .ctls[cn++] = (ub1)ipart4 | Lan_im; # 1i
  ot -root nlit .ctls[cn++] = ipart4; # 1 digit

nlit nlit 3+ int digits
  0
  nm
  .. flitf0
  eE flitxs
  _
  j   root nlit .ctls[cn++] = Lan_a | Lan_im;
  ot -root nlit .ctls[cn++] = Lan_a;

# ---------------------
# float literal fraction
# ---------------------
flitf0 start of flit fraction
  0
  nm  flitf  .  .ctls[cn] = min(n-N,15) | Lan_a;
  eE  flitxs
  j   root nlit .ctls[cn++] = Lan_a | Lan_im;
  ot -root nlit .ctls[cn++] = Lan_a;

flitf flit fraction digits
  nu
  _
  eE  flitxs
  j   root nlit .ctls[cn++] |= Lan_im;
  ot -root nlit .cn++;

# ---------------------
# float literal exp
# ---------------------
flitxs
  + flitx0
  - flitx0
  0 flitx0
  nm flitx

flitx0
  0
  _
  nm  flitx . .exdig=1;
  ot -root nlit .ctls[cn++] = Lan_a;

flitx
  nu . . .if (exdig++ > 5) lxerror(l,0,$S,$P,c,"excess exponent digits");
  _
  j   root nlit .exdig=min(exdig,3); ctls[cn++] = exdig << 4;
  ot -root nlit .exdig=min(exdig,3); ctls[cn++] = Lan_a | (exdig << 4);

# dummy 0e[+-]dd
nlite0
  pm nlite
  nu nlite

nlite
  nu
  ot -root nlit .ctls[cn++] = 0;

# ---------------------
# int literal binary
# ---------------------
ilitb0
  0
  1 ilitb . .i8 = 1;
  _

ilitb
  0 . . .i8 <<= 1;
  1 . . .i8 = i8 << 1 | 1;
  _
  ot -root nlit ilit

# ---------------------
# int literal hex
# ---------------------
ilitx0
  0
  nm ilitx . .i8 = c - '0';
  hx ilitx . .i8 = (c | 0x20) - 'a';
  _

ilitx
  0  . . .i8 <<= 4;
  nm . . .i8 = i8 <<4 | (c - '0');
  hx . . .i8 = i8 <<4 | ((c | 0x20) - 'a');
  _
  ot -root nlit ilit

# ---------------------
# string literal start
# ---------------------
slit0
  Q slitcat0 . .len = 0; # empty short slit ''
  \ -slit
  ot slit . .N=n; slitx = slitpos; slitpool[slitx++] = c;

# ---------------------
# string literal
# ---------------------
slit string literal
  Q slitcat0 . .len = n - N;
  \Q . . .slitpool[slitx++] = Q;
  \ slitesc
  ot . . .slitpool[slitx++] = c;

slitesc
  ot -slit . doesc

# ---------------------
# tentative slit concat
# ---------------------
slitcat0
  ws
  qq slit0
  ot -root slit slit
