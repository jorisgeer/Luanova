// dia.h - diag

enum Diamod { Dm_lex,Dm_syn,Dm_sem,Dm_var,Dm_cmd,Dm_count };

struct diamod {
  cchar ** names;
  ub2 *lvls;
  ub2 cnt;
};

extern struct diamod dia_lex;

// extern void diaini(enum Diamod mod,cchar ** dianames,ub2 *dialvls,ub2 tagcount);
extern int diaset(enum Msglvl lvl,cchar *list);
