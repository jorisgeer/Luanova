/* tok.h - lexer token defines

   generated by genlex 0.1.0-alpha 27 Sep 2022  9:37

   from lua.lex 0.1.0 27 Sep 2022  9:40 lua
   signature: @ 9c2a0bddc8353182 @ */

enum Packed8 Token {
  Tdo        =  0, Telse      =  1, Telseif    =  2, Tend       =  3, Tfalse     =  4, Tfor       =  5, Tfunction  =  6, Tgoto      =  7,
  Tif        =  8, Tin        =  9, Tlocal     = 10, Tnil       = 11, Tnot       = 12, Trepeat    = 13, Treturn    = 14, Tthen      = 15,
  Ttrue      = 16, Tuntil     = 17, Twhile     = 18, Tctlxfer   = 19, 
  T99_mrg    = 19, 
  T99_kwd    = 20,

  Tid        = 20, Tnlit      = 21, Tslit      = 22, 
  T0grp      = 22,
  Top        = 23, 
  T1grp      = 23,
  Taas       = 24, 
  T2grp      = 24,
  Tco        = 25, Tcc        = 26, 
  T3grp      = 26,
  Tsepa      = 27,
  Tcomma     = 28, Tro        = 29, Trc        = 30, Tso        = 31, T99_eof    = 31, Tsc        = 32, Tao        = 33, Tac        = 34, Tdas       = 35,
  Tcolon     = 36, Tdcol      = 37, Tdot       = 38, Tell       = 39, 
  T4grp      = 39,
  
  T99_count  = 40
};

#define Kwcnt 20
#define Tknamlen 8
#define Tkgrp 5

static const ub2 toknampos[21] = {
  0,3,8,13,16,20,23,26,31,37,40,43,46,49,52,55,59,65,70,74,78
};

static const char toknampool[78] = "id\0nlit\0slit\0op\0aas\0co\0cc\0sepa\0comma\0ro\0rc\0so\0sc\0ao\0ac\0das\0colon\0dcol\0dot\0ell\0";

enum Packed8 token {
  _tdo        =   0, _telse      =   1, _telseif    =   2, _tend       =   3, _tfalse     =   4, _tfor       =   5, _tfunction  =   6, _tgoto      =   7,
  _tif        =   8, _tin        =   9, _tlocal     =  10, _tnil       =  11, _tnot       =  12, _trepeat    =  13, _treturn    =  14, _tthen      =  15,
  _ttrue      =  16, _tuntil     =  17, _twhile     =  18, _tbreak     =  19, _tcontinue  =  20,
  t99_count  = 21
};

static const ub1  kwnamlens [20] = { 2 ,4 ,6 ,3 ,5 ,3 ,8 ,4 ,2 ,2 ,5 ,3 ,3 ,6 ,6 ,4 ,4 ,5 ,5 ,7  };
static const ub2  kwnamposs [20] = { 0 ,2 ,6 ,12,15,20,23,31,35,37,39,44,47,50,56,62,66,70,75,80 };
static const char kwnampool[87] = "doelseelseifendfalseforfunctiongotoifinlocalnilnotrepeatreturnthentrueuntilwhilectlxfer";

static const ub1 tokhrctl[20] = { 0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,2,1,3 };

static char hrtoknams[80] = "                    {   }   ;   ,   (   )   [   ]   <   >   =   :   ::  .   ... "; // 20 * 4

enum Packed8 Bltin {
  B_ENV            =   0, B_G              =   1, Bassert          =   2, Bcollectgarbage  =   3, Bdofile          =   4, Berror           =   5, Bgetmetatable    =   6, Bipairs          =   7,
  Bload            =   8, Bloadfile        =   9, Bnext            =  10, Bpairs           =  11, Bpcall           =  12, Bprint           =  13, Brawequalk       =  14, Brawget          =  15,
  Brawlen          =  16, Brawset          =  17, Bselect          =  18, Bsetmetatable    =  19, Btonumber        =  20, Btostring        =  21, Btype            =  22, B_VERSION        =  23,
  Bwarn            =  24, Bxpcall          =  25, Bcoroutine       =  26, Bclose           =  27, Bcreate          =  28, Bisyieldable     =  29, Bresume          =  30, Brunning         =  31,
  Bstatus          =  32, Bwrap            =  33, Byield           =  34, Brequire         =  35, Bpackage         =  36, Bconfig          =  37, Bcpath           =  38, Bloaded          =  39,
  Bloadlib         =  40, Bpath            =  41, Bpreload         =  42, Bsearchers       =  43, Bsearchpath      =  44, Bstring          =  45, Bbyte            =  46, Bchar            =  47,
  Bdump            =  48, Bfind            =  49, Bformat          =  50, Bgmatch          =  51, Bgsub            =  52, Blen             =  53, Blower           =  54, Bmatch           =  55,
  Bpack            =  56, Bpacksize        =  57, Brep             =  58, Breverse         =  59, Bsub             =  60, Bunpack          =  61, Bupper           =  62, Butf8            =  63,
  Bcharpattern     =  64, Bcodes           =  65, Bcodepoint       =  66, Boffset          =  67, Btable           =  68, Bconcat          =  69, Binsert          =  70, Bmove            =  71,
  Bremove          =  72, Bsort            =  73, Bmath            =  74, Babs             =  75, Bacos            =  76, Basin            =  77, Batan            =  78, Bceil            =  79,
  Bcos             =  80, Bdeg             =  81, Bexp             =  82, Bflor            =  83, Bfmod            =  84, Bhuge            =  85, Blog             =  86, Bmax             =  87,
  Bmaxinteger      =  88, Bmin             =  89, Bmininteger      =  90, Bmodf            =  91, Bpi              =  92, Brad             =  93, Brandom          =  94, Brandomseed      =  95,
  Bsin             =  96, Bsqrt            =  97, Btan             =  98, Btointeger       =  99, Bult             = 100, Bio              = 101, Bflush           = 102, Binput           = 103,
  Blines           = 104, Bopen            = 105, Boutput          = 106, Bpopen           = 107, Bread            = 108, Btmpfile         = 109, Bwrite           = 110, Bfile            = 111,
  Bseek            = 112, Bsetvbuf         = 113, Bos              = 114, Bclock           = 115, Bdate            = 116, Bdifftime        = 117, Bexecute         = 118, Bexit            = 119,
  Bgetenv          = 120, Brename          = 121, Bsetlocale       = 122, Btime            = 123, Btmpname         = 124, Bdebug           = 125, Bgethook         = 126, Bgetinfo         = 127,
  Bgetlocal        = 128, Bgetregistry     = 129, Bgetupvalue      = 130, Bgetuservale     = 131, Bsethook         = 132, Bsetlocal        = 133, Bsetupvalue      = 134, Bsetuservalue    = 135,
  Btraceback       = 136, Bupvalueid       = 137, Bupvaluejoin     = 138,
  B99_count        = 139
};

static const ub1 hibltlen = 14;
static const ub1 mibltlen = 6;
enum Packed8 Dunder {
  Dadd       =   0, Dsub       =   1, Dmul       =   2, Ddiv       =   3, Dmod       =   4, Dpow       =   5, Dunm       =   6, Didiv      =   7,
  Dband      =   8, Dbor       =   9, Dbxor      =  10, Dbnot      =  11, Dshl       =  12, Dshr       =  13, Dconcat    =  14, Dlen       =  15,
  Deq        =  16, Dlt        =  17, Dle        =  18, Dindex     =  19, Dnewindex  =  20, Dcall      =  21, Dgc        =  22, Dclose     =  23,
  Dmode      =  24, Dname      =  25,
  D99_count  = 26
};

static const ub1 hidunlen = 8;
static const char tkwnampool[135] = "do  elseelseif  end false   for functiongotoif  in  local   nil not repeat  return  thentrueuntil   while   break   continueunknown_kw\0";

static const ub2  tkwnamposs[ 22] = { 0,4,8,16,20,28,32,40,44,48,52,60,64,68,76,84,88,92,100,108,116,124 };

static const ub1  tkwnamlens[ 22] = { 2,4,6,3,5,3,8,4,2,2,5,3,3,6,6,4,4,5,5,5,8 };

static const ub4 kwnamhsh = 0xc9e67151;

static const char bltnampool[1015] = "_ENV_G  assert  collectgarbage  dofile  error   getmetatableipairs  loadloadfilenextpairs   pcall   print   rawequalk   rawget  rawlen  rawset  select  setmetatabletonumbertostringtype_VERSIONwarnxpcall  coroutine   close   create  isyieldable resume  running status  wrapyield   require package config  cpath   loaded  loadlib pathpreload searchers   searchpath  string  bytechardumpfindformat  gmatch  gsublen lower   match   packpacksizerep reverse sub unpack  upper   utf8charpattern codes   codepoint   offset  table   concat  insert  moveremove  sortmathabs acosasinatanceilcos deg exp florfmodhugelog max maxinteger  min mininteger  modfpi  rad random  randomseed  sin sqrttan tointeger   ult io  flush   input   lines   openoutput  popen   readtmpfile write   fileseeksetvbuf os  clock   datedifftimeexecute exitgetenv  rename  setlocale   timetmpname debug   gethook getinfo getlocalgetregistry getupvalue  getuservale sethook setlocalsetupvalue  setuservaluetraceback   upvalueid   upvaluejoinunknown_blt\0";

static const ub2  bltnamposs[140] = { 0,4,8,16,32,40,48,60,68,72,80,84,92,100,108,120,128,136,144,152,164,172,180,184,192,196,204,216,224,232,244,252,260,268,272,280,288,296,304,312,320,328,332,340,352,364,372,376,380,384,388,396,404,408,412,420,428,432,440,444,452,456,464,472,476,488,496,508,516,524,532,540,544,552,556,560,564,568,572,576,580,584,588,592,596,600,604,608,612,624,628,640,644,648,652,660,672,676,680,684,696,700,704,712,720,728,732,740,748,752,760,768,772,776,784,788,796,800,808,816,820,828,836,848,852,860,868,876,884,892,904,916,928,936,944,956,968,980,992,1003 };

static const ub1  bltnamlens[140] = { 4,2,6,14,6,5,12,6,4,8,4,5,5,5,9,6,6,6,6,12,8,8,4,8,4,6,9,5,6,11,6,7,6,4,5,7,7,6,5,6,7,4,7,9,10,6,4,4,4,4,6,6,4,3,5,5,4,8,3,7,3,6,5,4,11,5,9,6,5,6,6,4,6,4,4,3,4,4,4,4,3,3,3,4,4,4,3,3,10,3,10,4,2,3,6,10,3,4,3,9,3,2,5,5,5,4,6,5,4,7,5,4,4,7,2,5,4,8,7,4,6,6,9,4,7,5,7,7,8,11,10,11,7,8,10,12,9,9,11 };

static const char dun0nampool[132] = "add sub mul div mod pow unm idivbandbor bxorbnotshl shr concat  len eq  lt  le  index   newindexcallgc  close   modenameunknown_dun\0";

static const ub2  dunnamposs[ 27] = { 0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,64,68,72,76,80,88,96,100,104,112,116,120 };

static const ub1  dunnamlens[ 27] = { 3,3,3,3,3,3,3,4,4,3,4,4,3,3,6,3,2,2,2,5,8,4,2,5,4,4 };

