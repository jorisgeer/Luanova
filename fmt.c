/* fmt.c - printf-style string formatting

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

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
// #include <math.h>

#include "base.h"

#include "fmt.h"

#include "chr.h"
#include "os.h"

#if defined __clang__
 #pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined __GNUC__
 #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#define Maxfmt 512

static char x1cnv(unsigned char x,char hex)
{
  return x + (x > 9 ? ('0' + hex - '9' - 1) : '0');
}

static void xbcnv(char *dst,unsigned char x,char hex)
{
  dst[0] = x1cnv(x >> 4,hex);
  dst[1] = x1cnv(x & 0xf,hex);
}

// basic %x
static char *xcnv(char *end,unsigned int x,char hex)
{
  ub1 c;

  do {
    c = (ub1)(x & 0xf);
    if (c > 9) {
      c = hex + (c - 10);
    } else c += '0';
    *--end = c;
    x >>= 4;
  } while (x);

  return end;
}

static char *xlcnv(char *end,unsigned long x,char hex)
{
  ub1 c;

  do {
    c = (ub1)(x & 0xf);
    if (c > 9) {
      c = hex + (c - 10);
    } else c += '0';
    *--end = c;
    x >>= 4;
  } while (x);

  return end;
}

static char *xllcnv(char *end,unsigned long long x,char hex)
{
  ub1 c;

  do {
    c = (ub1)(x & 0xf);
    if (c > 9) {
      c = hex + (c - 10);
    } else c += '0';
    *--end = c;
    x >>= 4;
  } while (x);

  return end;
}

// basic %u
static char *ucnv(char *end,unsigned int x)
{
  do *--end = (ub1)((x % 10) + '0'); while (x /= 10);

  return end;
}

char *utoa(char *end,ub4 x)
{
  *--end = 0;
  return ucnv(end,x);
}

char *itoa(char *end,sb4 x)
{
  char *p;
  *--end = 0;
  if (x < 0) {
    x = -x;
    p = ucnv(end,(ub4)x);
    *--p = '-';
    return p;
  } else return ucnv(end,(ub4)x);
}

static char *ulcnv(char *end,unsigned long x)
{
  do *--end = (ub1)((x % 10) + '0'); while (x /= 10);

  return end;
}

char *ultoa(char *end,unsigned long x) {
  *--end = 0;
  return ulcnv(end,x);
}

static char *ullcnv(char *end,unsigned long long x)
{
  do *--end = (ub1)((x % 10) + '0'); while (x /= 10);

  return end;
}

// human-readable %u, 2.3G
static char *Ucnv(char *end,ub4 x)
{
  ub4 x1,x2;
  char scale;

  if (x >= 1024U * 1024U * 1024U) { x1 = x >> 30; x2 = x >> 20; scale = 'G'; }
  else if (x >= 1024U * 1024U) { x1 = x >> 20; x2 = x >> 10; scale = 'M'; }
  else if (x >= 1024U) { x1 = x >> 10; x2 = x; scale = 'K'; }
  else {
    *--end = ' ';
    return ucnv(end,x);
  }
  *--end = scale;

  x2 = (x2 & 0x3ff) / 100;

  *--end = (ub1)min(x2,9) + '0';
  *--end = '.';

  do *--end = (ub1)((x1 % 10) + '0'); while (x1 /= 10);

  return end;
}

static char *Ulcnv(char *end,unsigned long x)
{
  ub4 x1,x2;
  char scale;

  if (x == hi64) { memcpy(end - 4,"hi64",4); return (end - 4); }
  else if (x >= (1UL << 60)) { x1 = (ub4)(x >> 60); x2 = (ub4)((x >> 50) & 0xffff); scale = 'E'; }
  else if (x >= (1UL << 50)) { x1 = (ub4)(x >> 50); x2 = (ub4)((x >> 40) & 0xffff); scale = 'P'; }
  else if (x >= (1UL << 40)) { x1 = (ub4)(x >> 40); x2 = (ub4)((x >> 30) & 0xffff); scale = 'T'; }
  else if (x >= (1UL << 30)) { x1 = (ub4)(x >> 30); x2 = (ub4)((x >> 20) & 0xffff); scale = 'G'; }
  else {
    *--end = ' ';
    return Ucnv(end,(ub4)x);
  }
  *--end = scale;

  x2 = (x2 & 0x3ff) / 100;

  *--end = (ub1)min(x2,9) + '0';
  *--end = '.';

  do *--end = (ub1)((x1 % 10) + '0'); while (x1 /= 10);

  return end;
}

// simple %e
static char *ecnv(char *end, double x,ub2 prec,char expch)
{
#if 0
  double fexp,exp;
  ub8 xscale;
  char c;

  fexp = log10(x);
  if (fexp < 0) {     // |x| < 1.0
    exp = -floor(fexp);
    x *= pow(10,exp);
    end = ucnv(end,(ub4)exp);
    *--end = '-';
  } else if (fexp >= 1) { // |x| >= 10.0
    exp = floor(fexp);
    x /= pow(10,exp);
    end = ucnv(end,(ub4)exp);
  } else {
    exp = 0;
    *--end = '0';
  }
  *--end = expch;

  xscale = (ub8)(x * prec);
  end = ulcnv(end,xscale);
  c = *end;
  *end-- = '.';
  *end = c;
  return end;

#else
*--end = 0;
*--end = expch;
*--end = prec  + '0';
*--end = x == 0.0 ? '0' : '1';
  memcpy(end-10," %e todo ",9);
  return end-10;
#endif
}

#if 0
// simple %f
static ub4 fcnv(char *dst, ub4 len,double x)
{
  double fexp,exp;
  ub4 iexp;
  ub4 ix,n = 0,pos = 0;

  if (len < 24) return len;

  // trivia
  if (isnan(x)) { memcpy(dst,"#NaN",4); return 4; }
  else if (isinf(x)) { memcpy(dst,"#Inf",4); return 4; }
  else if (x > -1e-7 && x < 1e-7) { *dst = '0'; return 1; }
  else if (x < -4e9) { *dst++ = '-'; n = 1 + ucnv(dst,len,hi32,0,' '); return n; }
  else if (x > 4e9) return ucnv(dst,len,hi32,0,' ');

  if (x < 0) { dst[pos++] = '-'; x = -x; }

  fexp = log10(x);
  if (fexp < 0) {     // |x| < 1.0
    if (fexp < -7) { dst[pos] = '0'; return n + 1; }

    exp = floor(fexp);
    iexp = (ub4)-exp;

    dst[pos++] = '0'; dst[pos++] = '.';
    while (iexp > 1 && len - pos > 2) { dst[pos++] = '0'; if (iexp) iexp--; }

    x *= 1e7;

    if (x > 1) {
      if (len - pos < 16) return len;
      ix = (ub4)x;
      pos += ucnv(dst + pos,len,ix,0,'0');
    }
  } else { // |x| >= 1.0
    ix = (ub4)x;
    pos += ucnv(dst + pos,len,ix,0,'0');
    dst[pos++] = '.';
    x = (x - ix) * 1e+7;
    ix = (ub4)x;
    pos += ucnv(dst + pos,len,ix,7,'0');
  }
  return pos;
}
#endif

enum Lenmod { None, Long=0x10, Longlong=0x20, Short=0x40, Short1=0x80, Ldouble=0x100 };
enum Spec { Empty,Sdec,Dec,Oct,Hex,Char,String,Hexstr,Scifp,Fixfp,Mixfp,Hexfp,Ptr,Len,Speccnt };
enum Packed8 Flags { Noflag,Padleft=1,Altfmt=2,Dot3=4,Scaled=8,Ucfirst=0x10,Plural=0x20 };

/* todo
   decode fmt and args into internal list of specs and union for args
   determine len per field and provide per spec
   print fields that fit, and via buffer if partial
 */

ub4 myvsnprint(char *dst,ub4 pos,ub4 len,const char *fmt,va_list ap)
{
  const char *p;
  char *end,*org,*org2,*end2;
  ub4 slen,flen = 0,lim,ndig,ncnv=0;
  ub4 n = 0;
  ub2 wid;
  ub4 prec;
  enum Lenmod lenmod;
  enum Spec spec;

  unsigned int uval=0,*puval,uintwid = sizeof(int) * 8 - 1;
  unsigned long ulval=0,*pulval,ulongwid = sizeof(long) * 8 - 1;
  unsigned long long ullval=0,*pullval;
  long long llval;
  unsigned short *pusval;
  unsigned char *pucval;
  int ival;
  double fdval=0;
  long double fldval=0;
  char *pcval=nil,*pcend;
  void *vpval=nil;

  char c,c1,c2=0;
  // int fpclass;
  char pad,signch,hexch=0,expch=0,endc=0;
  bool iszero,lastone=0,isi,isf;
  int fpsign=0;
  enum Flags flags;
  bool morflg;
  char buf[Maxfmt];
  char buf2[Maxfmt];

  sassert((ub1)Long > (ub1)Speccnt,"len mods need to be mergable with specs");

  if (pos + 3 >= len) {
    n = snprintf(buf,Maxfmt,"fmt.%u fatal: snprintf pos %u at %u for '%.64s'\n",__LINE__,pos,len,fmt);
    oswrite(2,buf,n,nil);
    _Exit(1);
  }

  ulval = (unsigned long)fmt;
  if (ulval == 0) fmt = "(nil)";
  else if (ulval < 4096) fmt = "(int)";
  else if (ulval == hi32) fmt = "(hi32)";
  else if (ulval == hi64) fmt = "(hi64)";

  else if (*fmt == 0) { dst[pos] = 0; return 0; }
  else if ( (c1 = *fmt) != '%' && fmt[1] == 0) { // single char
    dst[pos] = c1;
    dst[pos+1] = 0;
    return 1;
  } else if (*fmt == '%' && fmt[1] == 's' && fmt[2] == 0) { // shorthand for lone "%s"
    pcval = va_arg(ap,void *);
    ulval = (unsigned long)pcval;
    if (ulval == 0) pcval = "(nil)";
    else if (ulval < 4096) pcval = "(int)";
    else if (ulval == hi64) pcval = "(hi64)";
    n = pos;
    while (*pcval && pos + 2 < len) {
      dst[pos++] = *pcval++;
    }
    dst[pos] = 0;
    return pos - n;
  }

  len -= pos; dst += pos; len--;

  p = fmt;

  while (*p && len > n + 2) {
    c1 = *p++;

    // all conversions
    if (c1 == '%') {
      if (*p == '%') { p++; dst[n++] = '%'; continue; }

      wid = 0; pad = ' ';
      prec = hi32;
      morflg=1;
      flags = Noflag;
      signch = 0;

     // flags
     do {
      c2 = *p++;
      switch(c2) {
      case '-': flags |= Padleft; break;
      case '+': signch = '+'; break;
      case ' ': if (signch == 0) signch = ' '; break;
      case '#': flags |= Altfmt; break;
      case '0': pad = '0'; break;
      case '\'': flags |= Dot3; break;
      case 'I': flags = (flags & ~Dot3) | Scaled; break;
//      case 'I': break; // ignore

      default: morflg = 0;
      }
     } while (morflg);

      // width as arg
      if (c2 == '*') {
        ival = va_arg(ap,int);
        if (ival < 0) { flags |= Padleft; wid = (ub2)-ival; }
        else wid = (ub2)min(ival,Maxfmt);
        c2 = *p++;
      }

      // width
      while (c2 >= '0' && c2 <= '9') {
        if (wid < 4096) wid = wid * 10 + (ub2)(c2 - '0');
        c2 = *p++;
      }
      if (wid > len - n) wid = len - n;

      // precision
      if (c2 == '.') {
        prec = 0;
        c2 = *p++;
        if (c2 == '*') {
          ival = va_arg(ap,int);
          if (ival >= 0) prec = (ub4)ival;
          else prec = hi32;
          c2 = *p++;
        } else {
          while (c2 >= '0' && c2 <= '9') {
            if (prec < (1U << 26)) prec = prec * 10 + (ub4)(c2 - '0');
            c2 = *p++;
          }
        }
      }

      end = buf + Maxfmt;
      org = nil;

      // modifiers
      lenmod = None;
      switch (c2) {
      case 'l':
        if (p[0] == 'l') { p++; lenmod = Longlong; }
        else lenmod = Long;
        break;
      case 'h':
        if (p[0] == 'h') { p++; lenmod = Short1; }
        else lenmod = Short;
        break;
      case 'j': lenmod = Longlong; break;
      case 'z': lenmod = Long;     break;
      case 't': lenmod = Long;     break;
      case 'L': lenmod = Ldouble;  break;
      }
      if (lenmod != None) c2 = *p++;

      // conversions
      pcval = nil;
      hexch = 'a';
      isi = isf = 0;

      spec = Empty;
      switch(c2) {
      case 'd':
      case 'i': isi = 1; spec = Sdec;   break;
      case 'o': isi = 1; spec = Oct;    break;
      case 'u': isi = 1; spec = Dec;    if (*p == '`') { flags |= Scaled; p++; } break;
      case 'x': isi = 1; spec = Hex;    break;
      case 'X': isi = 1; spec = Hex;    hexch = 'A'; break;

      case 'f':
      case 'F': isf = 1; spec = Fixfp;  break;
      case 'e':
      case 'E': isf = 1; spec = Scifp;  expch = c2; break;
      case 'g':
      case 'G': isf = 1; spec = Mixfp;  expch = c2 - 2; break;
      case 'a': isf = 1; spec = Hexfp;  break;
      case 'A': isf = 1; spec = Hexfp;  hexch = 'A'; break;

      case 'c': spec = Char;   break;

      case 's': spec = String;
        endc = 0;
        if (*p == '`') {
          if (p[1] == 's') { flags |= Plural; p += 2; }
          else if (p[1] == 'z') { endc = ' '; p += 2; }
        }
        break;

      case 'p': spec = Ptr;    break;
      case 'P': spec = Hexstr; break;
      case 'n': spec = Len;    break;

      // strerror(errno). no field formatting
      case 'm': pcval = getoserr(&ival);
                if (ival == 0) break;
                while (n < len && *pcval) dst[n++] = *pcval++;
                pcval = itoa(buf + Maxfmt,ival);
                *--pcval = '('; *--pcval = ' ';
                while (n < len && *pcval) dst[n++] = *pcval++;
                dst[n++] = ')';
                break;

      default:  dst[n++] = '%'; dst[n++] = c2; // unknown spec: print as-is
      }
      if (spec == Empty) continue;
      else if (isi && lenmod == Ldouble) lenmod = None;
      else if (isf && lenmod != Ldouble) lenmod = None;

     } else { // plain char
       dst[n++] = c1;
       continue;
     }

     if (n + 1 >= len) { dst[len-1] = 0; return n; }

     if (spec != Sdec && spec != Dec && spec != Fixfp && spec != Mixfp) flags = flags & ~Dot3;

     // get args and handle trivia
     switch (spec) {

     case Sdec:
     case Oct:
     case Dec:
     case Hex: if (lenmod == Long) {
                 ulval = va_arg(ap,unsigned long); if (ulval <= hi32) { uval = (ub4)ulval; lenmod = None; }
               } else if (lenmod == Longlong) {
                 ullval = va_arg(ap,unsigned long long);
                 if (ullval <= hi32) { uval = (ub4)ullval; lenmod = None; }
                 else if (ullval <= hi64) { ulval = (unsigned long)ullval; lenmod = Long; }
               } else {
                 lenmod = None; uval = va_arg(ap,unsigned int);
               }

               if (lenmod == None) { iszero = (uval == 0); lastone = (uval == 1); }
               else { iszero = 0; lastone = 0; }

               if (prec != hi32) {
                 pad = ' ';
                 if (prec == 0 && iszero) { spec = Empty; }
                 else prec = min(prec, min(len - n,Maxfmt));
               } else if (iszero && wid == 0 && signch == 0 && !(flags & Altfmt)) {
                 dst[n++]= '0';
                 spec = Empty;
               } else prec = 1;
               break;

     case Char: if (lenmod != Short && lenmod != Short1) lenmod = None;
                uval = va_arg(ap,unsigned int);
                if (wid < 2) { dst[n++] = (ub1)uval; spec = Empty; }
                break;

     case Fixfp:
     case Scifp:
     case Mixfp:
     case Hexfp:
#if 0
                 if (lenmod == Ldouble) {
                   fldval = va_arg(ap,long double);
                   fpclass = fpclassify(fldval); fpsign = signbit(fldval);
                 } else {
                   fdval = va_arg(ap,double);
                   fpclass = fpclassify(fdval); fpsign = signbit(fdval); lenmod = None;
                 }
                 if (prec == hi32) prec = 6;
                 prec = min(prec, min(len - n,32));
                 pcval = nil;
                 switch(fpclass) {
                   case FP_NAN: pcval = "#Nan";  break;
                   case FP_INFINITE: pcval = fpsign ? "-Inf" : "Inf"; spec = String; lenmod = None; break;
                   case FP_ZERO: pcval = "0"; break;
                   case FP_SUBNORMAL: pcval = "#0"; break;
                   case FP_NORMAL: if (fpsign) {
                     if (lenmod == Ldouble) fldval = -fldval; else fdval = -fdval;
                   }
                   break;
                 }
#else
                 if (lenmod == Ldouble) {
                   fldval = va_arg(ap,long double);
                   lenmod = None;
                   fdval = (double)fldval;
                 } else {
                   fdval = va_arg(ap,double);
                 }
#endif
                 if (pcval) {
                   spec = String; lenmod = None;
                 }
                 break;

     case String:
     case Hexstr: pcval = va_arg(ap,void *);
                  if (pcval == nil) pcval = "(nil)";
                  else if (pcval == dst) pcval = "(=dst)";

                  if (prec == 0) { spec = Empty; lenmod = None; break;  }
                  ulval = (unsigned long)pcval;
                  if (ulval == 0) pcval = "(nil)";
                  else if (ulval < 4096) pcval = "(int)";
                  else if (ulval == hi64) pcval = "(hi64)";
                  else if (wid < 2 && spec == String) {
                    if (*pcval == 0) spec = Empty;
                    else if (pcval[1] == 0) { dst[n++] = *pcval; spec = Empty; }
                  }
                  if (lenmod != Short && lenmod != Short1) lenmod = None;
                  prec = min(prec,len - n - 1); // includes from hi32
                  break;

     case Ptr: vpval = va_arg(ap,void *);
               ulval = (unsigned long)vpval;
               if (ulval <= hi32) { uval = (ub4)ulval; lenmod = None; }
               else lenmod = Long;
               spec = Hex;
               flags |= Altfmt;
               prec = 1;
               break;

     case Len: vpval = va_arg(ap,void *);
               if (lenmod == Ldouble) lenmod = None;
               break;
     case Empty: case Speccnt: break;
     }
     if (spec == Empty) continue;

     // conversions
     switch ((ub2)spec | (ub2)lenmod) {

     case Sdec:
       if (uval & (1U << uintwid)) { org = ucnv(end,hi32 - uval + 1); signch = '-'; }
       else org = ucnv(end,uval);
     break;

     case Sdec|Long:
       if (ulval & (1UL << ulongwid)) { org = ulcnv(end,hi64 - ulval + 1); signch = '-'; }
       else org = ulcnv(end,ulval);
     break;

     case Sdec|Longlong:
       llval = (long long)ullval;
       if (llval < 0) { ullval = (unsigned long long)-llval; signch = '-'; }
       org = ullcnv(end,ullval);
     break;

     case Dec:
       if (flags & Scaled) org = Ucnv(end,uval);
       else org = ucnv(end,uval);
     break;

     case Dec|Long:
       if (flags & Scaled) org = Ulcnv(end,ulval);
       else org = ulcnv(end,ulval);
     break;

     case Dec|Longlong:
       org = ullcnv(end,ullval);
     break;

     case Hex:
       org = xcnv(end,uval,hexch);
     break;

     case Hex|Long:
       org = xlcnv(end,ulval,hexch);
     break;

     case Hex|Longlong:
       org = xllcnv(end,ullval,hexch);
     break;

     case Char: // trivia done
       org = end - 1; *org = uval;
     break;

     case Char|Long: dst[n++] = uval; break; // todo

     case String:
     case String|Long:
       if (wid == 0) {
         while (prec && *pcval && *pcval != endc) { dst[n++] = *pcval++; prec--; }
         if ( (flags & Plural) && lastone == 0) dst[n++] = 's';
       } else if (flags & Padleft) {
         pcend = pcval;
         while (prec-- && *pcend && *pcend != endc) dst[n++] = *pcend++;
         slen = (ub4)(pcend - pcval);
         wid = min(wid,len - n - slen);
         if (wid > slen) { memset(dst+n,' ',wid-slen); n += wid - slen; }
       } else {
         pcend = pcval;
         while (prec-- && *pcend && *pcend != endc) pcend++;
         slen = (ub4)(pcend - pcval);
         wid = min(wid,(ub2)(len - n - slen));
         if (wid > slen) { memset(dst+n,' ',wid-slen); n += wid - slen; }
         memcpy(dst+n,pcval,slen); n += slen;
       }
       // if ( (flags & Ucfirst) && n != orgn && Ctab[(dst[orgn])] == Calpha) dst[orgn] &= 0xdf;
     break;

     case Hexstr:
       if (len - n < 4) break;
       prec = min(prec,(len - n - 2) / 2); // includes from hi32
       if (wid == 0) {
         dst[n] = '0'; dst[n+1] = hexch + ('x' - 'a');
         n += 2;
         while (prec-- && *pcval) {
           xbcnv(dst+n,*pcval++,hexch);
           n += 2;
         }
       } else if (flags & Padleft) {
         pcend = pcval;
         while (prec-- && *pcend) pcend++;
         slen = (ub4)(pcend - pcval) * 2;
         wid = min(wid,len - n - slen);
         if (wid > slen) { memset(dst+n,' ',wid-slen); n += wid - slen; }
         while (slen) {
           xbcnv(dst+n,*pcval++,hexch);
           n += 2; slen -= 2;
         }
       } else {
         pcend = pcval;
         while (prec-- && *pcend) { xbcnv(dst+n,*pcend++,hexch); n += 2; }
         slen = (ub4)(pcend - pcval) * 2;
         wid = min(wid,len - n - slen);
         if (wid > slen) { memset(dst+n,' ',wid-slen); n += wid-slen; }
       }
     break;

     case Fixfp: break;

     case Scifp: org = ecnv(end,fdval,prec,expch);
                 if (fpsign) *--org = '-';
                 break;

     case Mixfp:
     case Hexfp: break;

     case Len:
       puval = (unsigned int *)vpval;
       if (puval) *puval = n;
     break;

     case Len|Long:
       pulval = (unsigned long *)vpval;
       if (pulval) *pulval = n;
     break;

     case Len|Longlong:
       pullval = (unsigned long long *)vpval;
       if (pullval) *pullval = n;
     break;

     case Len|Short:
       pusval = (unsigned short *)vpval;
       if (pusval) *pusval = n;
     break;

     case Len|Short1:
       pucval = (unsigned char *)vpval;
       if (pucval) *pucval = n;
     break;

     case Ptr:
     break;
     } // switch spec | lenmod

     if (org == nil) continue;

     flen = (ub4)(end - org);

     // leading zeroes
     if (prec > flen && (spec == Sdec || spec == Oct || spec == Dec || spec == Hex)) {
       do *--org = '0'; while (prec > ++flen);
     }

     // postproc
     if ( (flags & Altfmt) && spec == Hex) {
       *--org = hexch + ('x' - 'a'); *--org = '0'; flen += 2;
     } else if (signch && (spec == Sdec || spec == Dec)) {
       *--org = signch; flen++;
     }

     if ( (flags & Dot3) && flen > 3) {
       end2 = org2 = buf2 + Maxfmt;
       ndig = 0;
       while (end > org) {
         c = *--end;
         *--org2 = c;
         if (c >= '0' && c <= '9') ndig++;
         if (ndig > 2) { *--org2 = '.'; ndig = 0; }
       }
       end = end2;
       org = org2;
       flen = (ub4)(end - org);
     }

     if (wid) { // pad
       if (wid > flen) {
         lim = len - n;
         wid = min(wid,lim);
         if (flags & Padleft) {
           if (flen) memcpy(dst+n,org,flen);
           memset(dst+n+flen,pad,wid-flen);
         } else {
           memset(dst+n,pad,wid-flen);
           if (flen) memcpy(dst+n+wid-flen,org,flen);
         }
         n += wid;
       } else if (flen) {
         memcpy(dst+n,org,flen);
         n += flen;
       }
     } else if (flen) {
       memcpy(dst+n,org,flen);
       n += flen;
     }
  }
  dst[n] = 0;

  return n;
}

ub4 __attribute__ ((format (printf,4,5))) mysnprintf(char *dst,ub4 pos,ub4 len,const char *fmt,...)
{
  va_list ap;
  ub4 n;
  char buf[256];

  va_start(ap, fmt);
  n = myvsnprint(dst,pos,len,fmt,ap);
  va_end(ap);

  if (pos + 3 + n >= len) {
    n = snprintf(buf,Maxfmt,"fmt.%u fatal: snprintf pos %u+%u at %u for '%.64s'\n",__LINE__,pos,n,len,chprints((const ub1 *)fmt,32));
    oswrite(2,buf,n,nil);
    _Exit(1);
  }

  return n;
}

int __attribute__ ((format (printf,3,4))) snprintf(char *dst,ub8 len,cchar *fmt,...)
{
  va_list ap;
  ub4 n;

  va_start(ap,fmt);
  n = myvsnprint(dst,0,(ub4)len,fmt,ap);
  va_end(ap);
  return (int)n;
}

#if 0
int __attribute__ ((format (printf,3,4))) __snprintf_chk(char *dst,size_t len,cchar *fmt,...)
{
  va_list ap;
  ub4 n;

  if (len == 0) return 0;
  va_start(ap, fmt);
  n = myvsnprint(dst,0,len,fmt,ap);
  va_end(ap);
  return n;
}

int __attribute__ ((format (printf,2,3))) __sprintf_chk(char *dst,cchar *fmt,...)
{
  va_list ap;
  ub4 n;

  va_start(ap, fmt);
  n = myvsnprint(dst,0,1024,fmt,ap);
  va_end(ap);
  return n;
}
#endif
