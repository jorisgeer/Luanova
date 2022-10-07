/* ast.h - ast defines

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

// enum Role { Lhs,Rhs };

enum Vloc { Vlocl, Vglob, Vlit };
enum Typ { Yint, Yflt };

// generic
struct agen {
  enum Typ ty;
  ub1 res; // reg for result
  ub4 apos; // dbg
};

// term aka leaf
struct ilit {
  ub8 val;
};

struct flit {
  double val;
};

struct slit {
  ub4 val;
};

struct aid {
  ub4 id;
  ub1 lvl;
  // enum Role rol;
};

// derived
struct var {
  ub4 id;   // ni of aid
  ub4 tlo;  // 31-28 type  27-25 loc 24-0 offset
  ub4 ofs;
  ub1 lvl;
  // enum Role rol;
};

// nonterms
struct uexp {
  ub4 e;
  enum Uop op;
};

struct pexp {
  ub4 e;
  enum Bop op;
};

struct bexp {
  ub4 l,r;
  enum Bop op;
};

struct aexp {
  ub4 id;
  ub4 e;
};

struct asgnst {
  ub4 tgt;
  ub4 e;
};

struct blk {
  ub4 s;
  ub2 lvl;
};

struct param {
  ub4 id;
  ub4 t;
  ub4 def;
};

struct fndef {
  ub4 id;
  ub4 parvid;
  ub4 parfn;
  ub4 vid0,vidf0;
  ub4 pc0,pc1;
//  ub4 scid0,scid1;
  ub4 plst;
  ub4 blk;
  ub4 ret;
  ub2 argc;
  ub2 vidcnt;
};

struct aif {
  ub4 e;
  ub4 tb,fb;
  ub4 bcc,tail; // labels
  ub1 lvl;
};

struct witer {
  ub4 e;
  ub4 tb,fb;
  ub4 head,bcc,tail; // labels
  ub1 lvl;
};

struct stmt {
  ub4 s;
};

struct rexp {
  ub4 pos;
  ub4 opos;
  ub2 cnt;
  ub2 ocnt;
  ub2 nu;
  ub2 opndx;
  ub1 hit;
//  ub1 tlo,thi;
};

struct prmlst {
  ub4 pos;
  ub2 cnt;
};

struct stmtlst {
  ub4 pos;
  ub4 cnt;
};

struct ast {
  cchar *name;

  ub4 len;
  ub4 root;

  ub4 *nhs;
  ub4 *fpos;

  struct agen *gens;

  struct aid *ids;
  struct var *vars;

  struct ilit *ilits;
  struct flit *flits;
  struct slit *slits;

  struct pexp *pexps;
  struct uexp *uexps;
  struct bexp *bexps;
  struct aexp *aexps;

  struct asgnst *asgnsts;

  struct blk *blks;

  struct aif   *ifs;
  struct witer *witers;

  struct fndef *fndefs;
  struct param *prms;

  struct stmt *stmts;

  struct rexp *rexps;
  struct prmlst *prmls;
  struct stmtlst *stmtls;

  ub4 nid;
  ub4 uidcnt;
  ub4 aidcnt;
  ub4 histlstsiz;

  ub4 ndcnts[Acount+1];

  const ub1 *slitpool;

  ub8 *repool;
  ub4 *opspool;

  ub4 replen;

//  ub8 *scids;
//  ub2 *scidns;
//  ub4 nscid;

  ub2 hiblklvl;
  ub1 blkbit;
};
