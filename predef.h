/* predef.h - lexer definitions

   generated by genlex 0.1.0-alpha  3 Oct 2022  5:12

   from pre.lex 0.1.0  3 Oct 2022  4:46 lua
   options: code yes  tokens yes
 */
static const char specnam[] = "pre.lex";

static const char lexinfo[] = "pre.lex 0.1.0   3 Oct 2022  4:46 lua  code yes  tokens yes";

#define Cclen 4

/*
 AF  0   ' ABCDEFGHIJKLMNOPQSTUVWXYZ_abcdefghijklmnopqstuvwxyz'
 NM  1   ' 123456789'
 N0  2   ' 0'
 HT  3   ' \t '
 VT  4   ' \f '
 CR  5   ' \r '
 NL  6   ' \n '
 HS  7   ' # '
 SQ  8   ' ' '
 DQ  9   ' " '
 RR  10  ' r'
 BO  11  ' ( [ { '
 BC  12  ' ) ] } '
 DT  13  ' . '
 OP  14  ' ! % & * + - / < > @ ^ | ~ '
 PC  15  ' , : ; = ? '
 EOF 16  ' \0 '
 AN  1  + ' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz'
 NX  2  + ' . 0123456789_'
 XN  4  + ' 0123456789ABCDEF_abcdef'
 WS  0  - '   '
*/
#define x  17

static unsigned char ctab[256] = {
  16, x, x, x, x, x, x, x, x, 3, 6, x, 4, 5, x, x,  // 16, x, x, x, x, x, x, x, x,ht,nl, x,vt,cr, x, x,
   x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,  //  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
   x,14, 9, 7, x,14,14, 8,11,12,14,14,15,14,13,14,  //  x,op,dq,hs, x,op,op,sq,bo,bc,op,op,pc,op,dt,op,
   2, 1, 1, 1, 1, 1, 1, 1, 1, 1,15,15,14,15,14,15,  // n0,nm,nm,nm,nm,nm,nm,nm,nm,nm,pc,pc,op,pc,op,pc,
  14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // op,af,af,af,af,af,af,af,af,af,af,af,af,af,af,af,
   0, 0, x, 0, 0, 0, 0, 0, 0, 0, 0,11, x,12,14, 0,  // af,af, x,af,af,af,af,af,af,af,af,bo, x,bc,op,af,
   x, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //  x,af,af,af,af,af,af,af,af,af,af,af,af,af,af,af,
   0, 0,10, 0, 0, 0, 0, 0, 0, 0, 0,11,14,12,14, x,  // af,af,rr,af,af,af,af,af,af,af,af,bo,op,bc,op, x,
   x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,  //  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
   x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,  //  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
   x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,  //  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
   x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,  //  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // af,af,af,af,af,af,af,af,af,af,af,af,af,af,af,af,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // af,af,af,af,af,af,af,af,af,af,af,af,af,af,af,af,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // af,af,af,af,af,af,af,af,af,af,af,af,af,af,af,af,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x, x
};
#undef x

static unsigned char utab[256] = {
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,2  ,0  ,
  7  ,7  ,7  ,7  ,7  ,7  ,7  ,7  ,7  ,7  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,5  ,5  ,5  ,5  ,5  ,5  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,
  1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,0  ,0  ,0  ,0  ,7  ,
  0  ,5  ,5  ,5  ,5  ,5  ,5  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,
  1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,0  ,0  ,0  ,0  ,0  ,
  1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,
  1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,
  1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,
  1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,
  1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,
  1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,
  1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,
  1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,1  ,0  ,0  
};

