/* base.h - base definitions

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

#ifndef __STDC__
 #error "missing __STDC__ : require iso c 99+"
#endif
#ifndef __STDC_VERSION__
 #error "missing __STDC_VERSION__ : require iso c 99+"
#endif
#if __STDC_VERSION__ < 199901L
 #error "require iso c 99+"
#endif

typedef _Bool bool;
typedef unsigned char  ub1;
typedef unsigned short ub2;
typedef unsigned int   ub4;
typedef unsigned long  ub8;

typedef signed char sb1;
typedef short sb2;
typedef int   sb4;
typedef long  sb8;

typedef const char cchar;

#define ub4siz 4

#define Version_maj 0
#define Version_min 1
#define Version_pat 2
#define Version_phase "dev-a"

// handful of useful macros
#define hi16 0xffff
#define hi20 0xfffff
#define hi24 0xffffff
#define hi28 0xfffffff
#define hi32 0xffffffffU
#define hi56 0xffffffffffffffUL
#define hi64 0xffffffffffffffffUL

#define Bit31 0x80000000U
#define Bit63 0x8000000000000000UL

#define Fname 256
#define Pathname 4096

#ifndef max
 #define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
 #define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef nil
 #define nil (void*)0
#endif

// c11 langage only
#if __STDC_VERSION__ >= 201101L
 #define sassert(expr,msg) _Static_assert((expr),msg)
 #define Func __func__
 #define Noret _Noreturn

#else
 #define sassert(expr,msg) assert((expr),msg)
 #define Func ""
 #define Noret
 #define quick_exit(c) _Exit(c)
#endif

#if defined __clang__ && defined __clang_major__
 #define Packed8 __attribute__((packed))
 #define Fallthrough __attribute__ ((fallthrough));
 #define Mallike __attribute__ ((malloc))

 #pragma clang diagnostic error "-Wimplicit-function-declaration"
 #pragma clang diagnostic error "-Wincompatible-pointer-types"
 #pragma clang diagnostic error "-Wconditional-uninitialized"
 #pragma clang diagnostic error "-Wuninitialized"
 #pragma clang diagnostic error "-Wmissing-field-initializers"
 #pragma clang diagnostic error "-Wmissing-prototypes"
 #pragma clang diagnostic error "-Wint-conversion"

// #pragma clang diagnostic ignored "-Wunused-variable"
// #pragma clang diagnostic ignored "-Wunused-parameter"
// #pragma clang diagnostic ignored "-Wunused-macros"

 #pragma clang diagnostic ignored "-Wchar-subscripts"
 #pragma clang diagnostic ignored "-Wimplicit-int-conversion"
 #pragma clang diagnostic ignored "-Wpadded"
 #pragma clang diagnostic ignored "-Wpointer-sign"
 #pragma clang diagnostic ignored "-Wempty-translation-unit"

 #ifdef __has_feature
  #if __has_feature(address_sanitizer)
   #define Asan
  #endif
 #endif

#elif defined __GNUC__ && __GNUC__ > 3
 #define Packed8 __attribute__((packed))
 #define Fallthrough __attribute__ ((fallthrough));
 #define Mallike __attribute__ ((malloc))
 #define Inline __attribute__ ((always_inline))

// #pragma GCC diagnostic ignored "-Wattributes"
// #pragma GCC diagnostic ignored "-Wconversion"
// #pragma GCC diagnostic ignored "-Wunused-variable"
// #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
// #pragma GCC diagnostic ignored "-Wpointer-sign"

 #pragma GCC diagnostic warning "-Wdisabled-optimization"

 #pragma GCC diagnostic error "-Wshadow"
 #pragma GCC diagnostic error "-Wmaybe-uninitialized"
 #pragma GCC diagnostic error "-Wimplicit-function-declaration"
 #pragma GCC diagnostic error "-Wmissing-field-initializers"
 #pragma GCC diagnostic error "-Wvla"

 #ifdef __SANITIZE_ADDRESS__
  #define Asan
 #endif

#else
 #define Packed8
 #define Attribute(name)
 #define Fallthrough
 #define Inline inline
#endif

extern ub4 str2ub4(const char *s, ub4 *pv);
extern ub4 hex2ub4(const char *s, ub4 *pv);
extern int hex2ub8(const char *s, ub8 *pv);
extern ub4 bstr2ub4(const char *s, ub4 *pv);
extern ub1 atox1(ub1 c);
extern ub4 atou(cchar *p,ub2 len);

extern ub4 sat32(ub8 x,ub8 y);

extern ub4 nxpwr2(ub4 x,ub1 *bit);
extern ub2 nxbit(ub4 x);

extern ub1 msb(ub4 x);
extern ub2 msb8(ub8 x);
extern ub2 cntbits(ub4 x);
extern ub2 cntbits8(ub8 x);
extern ub2 expndx(ub2 x,ub2 lim);

extern Noret void doexit(int code);

extern int inibase(void);

enum Shsrcfile { Shsrc_ast,Shsrc_eval,Shsrc_msg,
  Shsrc_pre,Shsrc_lex,
  Shsrc_syn,Shsrc_main,Shsrc_time,Shsrc_util,
  Shsrc_vm,Shsrc_vmrun,Shsrc_mem,
  Shsrc_map,
  Shsrc_base,Shsrc_dia,Shsrc_os,Shsrc_net,
  Shsrc_lex1,
  Shsrc_genir,Shsrc_genlex,Shsrc_gensyn,
  Shsrc_report,
  Shsrc_count};

enum Pass { Lexpas=1,Synpas=2,Astpas=4 };

// program-wide global vars go here
struct globs {
  cchar *prgnam;
  char pidnam[64];

  ub4 prgdtim,prgdmin;
  int retval;
  ub1 rununtil;
  bool errwarn;
  bool nocol;
  bool resusg;

  bool runast;
  ub2 emit;
  ub2 trace;
  ub2 log; // unset
  ub2 abr;

  ub2 msglvl;

  ub4 maxvm;

  int pid;

  int sigint,sig;
  ub4 shfln;
};
extern struct globs globs;

struct myfile {
  int exist,direxist,alloced;
  int isdir,isfile;
  ub4 basename;
  ub4 len;
  ub4 xlen;
  unsigned long mtime;
  ub4 fln;
  char name[Fname];
  unsigned char *buf;
};

struct mysfile {
  int exist,direxist,alloced;
  int isdir,isfile;
  ub4 len;
  unsigned long mtime;
};

#define NVALGRIND
#ifdef NVALGRIND
 #define vg_set_undef(p,n)
 #define vg_chk_def(a,p,n)
#else
 #include <valgrind/memcheck.h>
 #define vg_set_undef(p,n) VALGRIND_MAKE_MEM_UNDEFINED((p),(n));
 #define vg_chk_def(a,p,n) (a) = VALGRIND_CHECK_MEM_IS_DEFINED((p),(n));
#endif
