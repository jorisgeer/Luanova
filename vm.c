/* vm.c - ir code virtual machine

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

#include <string.h>

#include "base.h"

#include "fmt.h"

static ub4 msgfile = Shsrc_vm;
#include "msg.h"

#include "irtyp.h"
#include "irdef.h"

enum Aty { At_u4,At_s4=0x10,At_u8=0x20,At_s8=0x30,At_f4=0x40,At_f8=0x50,At_str=0x60 };

extern void runir(const ub4 *prg,ub4 *mem);

void runir(const ub4 *prg,ub4 *mem)
{
  ub4 w;
  ub4 pc = 0;
  ub4 caljmp;

  enum Insgrp ig;
  enum Typ ty;
  enum Aty aty;
  enum Op op;
  enum Mod mod;
  enum Cc cc;
  enum Ctins ct;

  ub4 rd,rs1,rs2=0;
  ub4 ofs,oir;

  ub4 au4=0,bu4=0,cu4=0;
  sb4 as4=0,bs4=0,cs4=0;

  ub1 regsu1[Iregcnt];
  ub4 regsu4[Iregcnt];

  sb1 regss1[Iregcnt];
  sb4 regss4[Iregcnt];

  memset(regsu1,0,Iregcnt);
  memset(regsu4,0,Iregcnt * 4);

  memset(regss1,0,Iregcnt);
  memset(regss4,0,Iregcnt * 4);

  ub4 lim = 1000;

 do {

//  if (lim-- == 0) break;

  w = prg[pc];

  ig = w >> insgrpbit;

//  info("pc %u ig %u %x",pc,ig,w);

//  if (w == hi32) break;
  pc++;

  if (ig == Ig_ctl) { // jmp / cal / ret
    ct = (w >> ctinsbit) & ctinsmsk;
    if (ct != Ctbcc) {
      ofs = w & ccofsmsk;
      if (ofs == ccofsmsk) {
        info("end %x",w);
        break;
      } else {
//        info("ct %u",ct);
        switch (ct) {
          case Ctjmp: pc = ofs; break;
          case Ctcal:
          case Ctret:
          default: break;
        }
        continue;
      }
    } // jmp/cal/ret
  } // ctl

  ty = (w >> typbit) & typmsk;
  rd = (w >> regdbit) & regdmsk;

#if 1
  switch (ty) {

    case Ty_u1: case Ty_u2: case Ty_u4:

  switch (ig) {
    case Ig_ld: ofs = w & Ldoirmsk;
     switch (ty) {
       case Ty_u1: case Ty_u2: case Ty_u4:
//         info("ofs %x",ofs);
         regsu4[rd] = mem[ofs];
         break;
       default: break;
     }
     break;

    case Ig_st: ofs = w & Ldoirmsk;
     switch (ty) {
       case Ty_u1: case Ty_u2: case Ty_u4: mem[ofs] = regsu4[rd]; break;
       default: break;
     }
     break;

    case Ig_ari: op = (w >> aopbit) & aopmsk;

      rs1 = (w >> regs1bit) & regs1msk;

      bu4 = regsu4[rs1];

      mod = (w >> amodbit) & amodmsk;
      if (mod == Mimm) {
        oir = w & aoirmsk;
        cu4 = oir;
      } else {
        rs2 = (w >> regs2bit) & regs2msk;
        cu4 = regsu4[rs2];
      }

      switch (op) {
        case Oadd: au4 = bu4 + cu4; break;
        case Osub: au4 = bu4 - cu4; break;

        case Omul: au4 = bu4 * cu4; break;
        case Omod: au4 = bu4 % cu4; break;
        default: break;
      }
      regsu4[rd] = au4;

      break;

    case Ig_ctl: // only bcc
      cc = (w >> ccbit) & ccmsk;
      ofs = w & ccofsmsk;

      rs1 = (w >> regs1bit) & regs1msk;

      au4 = regsu4[rd]; bu4 = regsu4[rs1];

//      info("pc %u %x",pc,w);
      switch (cc) {
        case Cz: if (au4 == 0) pc = ofs; break;
        case Ceq: if (au4 == bu4) pc = ofs; break;
        default: break;
      }
//      info("pc %u %x",pc,w);
      break;

    case Ig_cnt: break;
  } // switch ig

    break;

    case Ty_s1: case Ty_s2:

  switch (ig) {
    case Ig_ld: ofs = w & Ldoirmsk;
     switch (ty) {
       case Ty_s1: case Ty_s2:
//         info("ofs %x",ofs);
         regss4[rd] = mem[ofs];
         break;
       default: break;
     }
     break;

    case Ig_st: ofs = w & Ldoirmsk;
     switch (ty) {
       case Ty_s1: case Ty_s2: mem[ofs] = regss4[rd]; break;
       default: break;
     }
     break;

    case Ig_ari: op = (w >> aopbit) & aopmsk;

      rs1 = (w >> regs1bit) & regs1msk;
      rs2 = (w >> regs2bit) & regs2msk;

      bs4 = regss4[rs1]; cs4 = regss4[rs2];

      mod = (w >> amodbit) & amodmsk;
      if (mod == Mimm) {
        oir = w & aoirmsk;
        cu4 = oir;
      }

      switch (op) {
        case Oadd: as4 = bs4 + cs4; break;
        case Osub: as4 = bs4 - cs4; break;

        case Omul: as4 = bs4 * cs4; break;
        case Omod: as4 = bs4 % cs4; break;
        default: break;
      }
      regss4[rd] = as4;

      break;

    case Ig_ctl: // only bcc
      cc = (w >> ccbit) & ccmsk;
      ofs = w & ccofsmsk;

      rs1 = (w >> regs1bit) & regs1msk;

      as4 = regss4[rd]; bs4 = regss4[rs1];

//      info("pc %u %x",pc,w);
      switch (cc) {
        case Cz: if (as4 == 0) pc = ofs; break;
        case Ceq: if (as4 == bs4) pc = ofs; break;
        default: break;
      }
//      info("pc %u %x",pc,w);
      break;

    case Ig_cnt: break;
  } // switch ig

    break;

    default: break;
  }

#else

  switch (ig) {
    case Ig_ld: ofs = w & Ldoirmsk;
     switch (ty) {
       case Ty_u1: case Ty_u2: case Ty_u4:
//         info("ofs %x",ofs);
         regsu4[rd] = mem[ofs];
         break;
       case Ty_s1: case Ty_s2: regss4[rd] = mem[ofs]; break;
       default: break;
     }
     break;

    case Ig_st: ofs = w & Ldoirmsk;
     switch (ty) {
       case Ty_u1: case Ty_u2: case Ty_u4: mem[ofs] = regsu4[rd]; break;
       case Ty_s1: case Ty_s2: mem[ofs] = regss4[rd]; break;
       default: break;
     }
     break;

    case Ig_ari: op = (w >> aopbit) & aopmsk;

  rs1 = (w >> regs1bit) & regs1msk;
  rs2 = (w >> regs2bit) & regs2msk;

  switch (ty) {
    case Ty_u1: case Ty_u4: bu4 = regsu4[rs1]; cu4 = regsu4[rs2]; aty = At_u4; break;
    case Ty_s1: case Ty_s2: bs4 = regss4[rs1]; cs4 = regss4[rs2]; aty = At_s4; break;
    case Ty_dyn:
    default: aty = At_f4; break;
  }

      mod = (w >> amodbit) & amodmsk;
      if (mod == Mimm) {
        oir = w & aoirmsk;
        switch (aty) {
          case At_u4: cu4 = oir; break;
          case At_s4: cs4 = oir; break;
          default: break;
        }
      }

      switch ( (ub4)op | (ub4)aty) {// 14 op x 7 ty * 8b
        case (Oadd | At_u4): au4 = bu4 + cu4; break;
        case (Oadd | At_s4): as4 = bs4 + cs4; break;
        case (Osub | At_u4): au4 = bu4 - cu4; break;

        case (Omul | At_u4): au4 = bu4 * cu4; break;
        case (Omod | At_u4): au4 = bu4 % cu4; break;
        default: break;
      }
      switch (ty) {
        case Ty_u1: case Ty_u4: regsu4[rd] = au4; break;
        case Ty_s1: case Ty_s2: regss4[rd] = as4; break;
      default: break;
      }

      break;

    case Ig_ctl: // only bcc
      cc = (w >> ccbit) & ccmsk;
      ofs = w & ccofsmsk;

      rs1 = (w >> regs1bit) & regs1msk;

      switch (ty) {
        case Ty_u1: case Ty_u4: au4 = regsu4[rd]; bu4 = regsu4[rs1]; aty = At_u4; break;
        case Ty_s1: case Ty_s2: as4 = regss4[rd]; bs4 = regss4[rs1]; aty = At_s4; break;
        case Ty_dyn:
        default: aty = At_f4; break;
      }

//      info("pc %u %x",pc,w);
      switch ( (ub4)cc | (ub4)aty) {// 7 op x 7 ty * 8b
        case (Cz | At_u4): if (au4 == 0) pc = ofs; break;
        case (Cz | At_s4): if (as4 == 0) pc = ofs; break;
        case (Ceq | At_u4): if (au4 == bu4) pc = ofs; break;
        default: break;
      }
//      info("pc %u %x",pc,w);
      break;

    case Ig_cnt: break;
  } // switch ig
#endif

 } while (1);

}

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

 -- ariops -- 10 1k
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
