/* irtyp.h - intermediate code

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

enum Opsiz { Siz0, Siz4, Siz8, Sizcnt };

enum Insgrp { Ig_ld,Ig_st,Ig_ari,Ig_ctl,Ig_cnt };

enum Ldins { Ld_bas,Ld_shl,Ld_typ,Ld_imm,Ld_cnt };
enum Stins { St_bas,St_shl,St_typ,St_imm,St_cnt };

enum Typ { Ty_tf,Ty_s1,Ty_u1,Ty_s2,Ty_u2,Ty_s4,Ty_u4,Ty_s8,Ty_u8,Ty_f4,Ty_f8,Ty_str,Ty_dyn,Ty_cnt };

enum Mod { Mreg,Mlofs,Mgofs,Mimm,Modcnt };

enum Op { Onot,Oneg,Oumin,Oupls,Oshl,Oshr,Oxor,Oor,Oand,Oadd,Osub,Omul,Odiv,Omod,Opcnt };

enum Ctins { Ctbcc,Ctjmp,Ctcal,Ctret,Ctcnt };

enum Cc { Cz,Ceq,Cne,Clt,Cltu,Cge,Cgeu,Cc_cnt };

#define Iregcnt 16

#define Ildshlbits 5

/*
 -- load and store --  8 256
 31-30  29-28  27-25  24-20  19-16  15-11-8-0
  ld     sub    typ    reg    mod    ofs/imm/ *reg

 31-30  29-28  27-25  24-20  19-16  15-10   9-5-0
  ld     sub    typ    reg    mod    shl    ofs/imm/ *reg

 31-30  29-28  27-25  24-22  21-20  19-17  16-14-0
  ld     sub    typ    reg    mod    typ    ofs/imm/reg

 31-30  29-28  27-25  24-22  21-20  - typ < 4
  ld     sub    typ    reg    imm

 31-30  29-28  27-25  24-22  21  20-0  +31-0 -- i4 @ x == 0
  ld     sub    typ    reg    x  imm     X

 31-30  29-28  27-25  24-22  21-20  19-0  +31-0 -- i8 @ x == 1
 31-30  29-28  27-25  24-22  21-20  19-0  +63-0 -- i8 @ x == 2
  ld     sub    typ    reg    x     imm     X

 31-30  29-28  27-25  24-22  21-20  19-0  +31-0 -- f48
  ld     sub    typ    reg    x     ofs     X  # flit pool ofs

  st as above

 -- ariops -- 2in + 2sub + 4typ + 4op
 31-30  29-26  25-23  22-18  17-15  14-12  11-9  8-0
  Ar     Typ    reg<    op    reg>   mod    reg> imm   # dr = sr1 binop sr2

 31-30  29-26  25-23  22-18  17-15  14-12  11-0
  Ar     Typ    reg<    op    reg>   mod    ofs        # dr = sr1 binop ofs

 31-30  29-26  25-23  22-18  17-15  14-12  11-0
  Ar     Typ    reg<    op    reg>   mod    imm        # dr = sr1 binop imm

 31-30  29-26  25-23  22-18  17-15  14-0
  ar     Typ    reg<    op    reg>   imm
 ar - uni

 -- ctl xfer --
 31-30  29-28  27-24  23-21  20-18 17-15  14 13-0
  ct    bcc     Typ    reg    reg   cc    Ir  dsp

 31-30  29-28  27-24  23-21  20-18 17-15  14 13-11  10-0
  ct    bcc     Typ    reg    reg   cc    Ir  reg   dsp

 31-30  29-28  27-25  25-0
  ct    jmp     mod    imm

 31-30  29-28  27-25  25-23  22-0
  ct    jmp     mod    reg    dsp

 31-30  29-28  27-25  25-0
  ct    jmp     loc    ofs

 31-30  29-28  24  23-0
  ct    cal    Ir  dsp

 31-30  29-28  24  23-21  20-0
  ct    cal    iR   reg    dsp

 ct - cal
   narg + frmsiz
   tlo*

 31-30  29-28  25-0
  ct    sub    frmsiz
 ct - ret


 cal + arg=pc
 ret + arg = frmofs

  reg.3

  typ.8  i1 i2 i4 i8 f4 f8 str dyn
  Typ.14 u1 s1 u2 s2 u4 s4 u8 s8 f4 f8 str obj dyn
  lssub.4 ofs shl typ imm

  ctsub.4 bcc jmp cal ret

  bop.13   + - * / % << >> & | == != < > && ||
  uop ! ~ []

  mod.8  locl glob imm reg*scale+dsp locl+ globl+ imm+

  cc.6    eq ne lt ltu ge geu

 */
