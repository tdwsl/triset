/* tri lang compiler - c version */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ins {
  unsigned short label, ins, param;
};

const char *key[] = {
  "++", "--",
  "&&", "||",
  "+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>",
  "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "=",
  "<=", ">=", "!=", "==", "<", ">",
  "{", "}", "(", ")", "[", "]",
  ",", ";", "!", "~", "\"", "'", ":", "?",
  "if", "while", "break", "continue", "return", "auto",
  "#define",
};

enum {
  PP=0, MM,
  ANAN, OROR,
  ADD, SUB, MUL, DIV, MOD, AND, OR, XOR, SHL, SHR,
  ADDEQ, SUBEQ, MULEQ, DIVEQ, MODEQ, ANDEQ, OREQ, XOREQ, SHLEQ, SHREQ, EQ,
  LE, GE, NEQ, EQU, LT, GT,
  LC, RC, LP, RP, LB, RB,
  COM, SEMI, NOT, INV, QUO, TICK, COL, TERN,
  IF, WHILE, BREAK, CONTINUE, RETURN, AUTO,
  DEFINE,
  ID0,
};

#define DCW 0xf0ff
#define DCL 0xf1ff
#define BLANK 0xf2ff

#define MAXIDS 2048
#define NUM0 (MAXIDS+ID0)
#define MAXLITS 2048

char *ids[MAXIDS];
char idbuf[16384];
char *idnext = idbuf;
int nids = 0;
unsigned nums[MAXLITS];
int nnums = 0;

char *labels[1024];
unsigned short lbla[1024];
int nlabels = 0;
struct ins mem[1024];
int nmem = 0;
int nand = 0, nor = 0, nif = 0, ntern;

#define MAXGLOBALS 300
#define MAXCONST 40

enum {
  UNDEF=-3,
  FUN=-2,
  DATA=-1,
  BSS=0,
  CONST=UNDEF-MAXCONST,
};

int globals[MAXGLOBALS];
int globalt[MAXGLOBALS];
int nglobals = 0;
int consts[MAXCONST];
int nconsts = 0;

int look;

#define MAXLOCALS 15

int locals[MAXLOCALS];
int nlocals = 0, argc;

FILE *fp, *out;
char ahc = 0;
int ln;
int id = 0;

void getNext(char *buf) {
  char c;
  int i = 0;
  for(;;) {
    if(ahc) { c = ahc; ahc = 0; }
    else c = fgetc(fp);
    if(c <= 32) {
      if(i) { buf[i] = 0; ahc = c; break; }
      if(c == EOF) { *buf = 0; break; }
      if(c == 10) ln++;
    } else if(strchr("+-*/%&|^<>=!~,;(){}[]\"':?", c)) {
      if(i) { buf[i] = 0; ahc = c; break; }
      buf[0] = c;
      if(strchr("(){};,~'\"[]", c)) { buf[1] = 0; break; }
      buf[1] = c = fgetc(fp); buf[2] = 0;
      if(buf[0] == '/' && c == '*') break;
      if(buf[0] == '*' && c == '/') break;
      if(c == '=') break;
      if(strchr("&|+-<>/", buf[0]) && c == buf[0]) {
        if(c == '>' || c == '<') {
          c = fgetc(fp);
          if(c == '=') { buf[2] = c; buf[3] = 0; }
          else ahc = c;
        }
        break;
      }
      buf[1] = 0; ahc = c;
      break;
    } else buf[i++] = c;
  }
}

char *str(int t) {
  static char buf[200];
  if(t == EOF) sprintf(buf, "EOF");
  else if(t < ID0) sprintf(buf, "%s", key[t]);
  else if(t < NUM0) sprintf(buf, "%s", ids[t-ID0]);
  else sprintf(buf, "%d", nums[t-NUM0]);
  return buf;
}

void errh() {
  printf("%d: ", ln);
  if(id != 0) printf("in %s: ", ids[id-ID0]);
}

void err(const char *s) {
  errh();
  printf("%s\n", s);
  exit(1);
}

void errf(const char *fmt, int t) {
  errh();
  printf(fmt, str(t)); printf("\n"); exit(1);
}

void nerr() { err("invalid integer literal"); }

unsigned number(char *s) {
  unsigned n = 0;
  char c;
  if(*s == '0') {
    s++;
    if(*s == 'x') while(*s) {
      c = *++s;
      n <<= 4;
      if(c >= '0' && c <= '9') n |= c-'0';
      else if(c >= 'a' && c <= 'f') n |= c-'a'+10;
      else if(c >= 'A' && c <= 'F') n |= c-'A'+10;
      else nerr();
    } else while(*s) {
      n <<= 3;
      c = *s++;
      if(c >= '0' && c <= '7') n |= c-'0';
      else nerr();
    }
  } else while(*s) {
    c = *s++;
    n *= 10;
    if(c >= '0' && c <= '9') n += c-'0';
    else nerr(0);
  }
  return n;
}

int next() {
  char buf[256];
  getNext(buf);
  if(!*buf) return look=EOF;
  for(int i = 0; i < ID0; i++)
    if(!strcmp(buf, key[i])) return look=i;
  unsigned n = 0;
  if(*buf >= '0' && *buf <= '9') {
    unsigned n = number(buf);
    for(int i = 0; i < nnums; i++)
      if(nums[i] == n) return look=NUM0+i;
    nums[nnums] = n;
    return look=nnums+++NUM0;
  }
  for(int i = 0; i < nids; i++)
    if(!strcmp(buf, ids[i])) return look=ID0+i;
  ids[nids] = idnext;
  strcpy(idnext, buf);
  idnext += strlen(idnext)+1;
  return look=nids+++ID0;
}

void dc(unsigned label, unsigned ins, unsigned param) {
  if(!label && nmem && mem[nmem-1].ins == BLANK) label = mem[--nmem].label;
  mem[nmem].ins = ins;
  mem[nmem].label = label;
  mem[nmem].param = param;
  nmem++;
}

void dins(int i, int a, int b, int c) {
  dc(0, i<<12|a<<8|b<<4|c, 0);
}

void op(int n, int o) {
  unsigned i = mem[nmem-2].ins;
  if((i&0xf000) == 0x9000 && (i>>8&15) == (i>>4&15) && (i>>4&15) == (i&15)) {
    nmem -= 2;
    dins(o, n, n, mem[nmem+1].ins&15);
  } else {
    dins(o+8, n, n, mem[nmem-1].ins&15);
  }
}

void inc() {
  int r = mem[nmem-1].ins>>8&15;
  dins(0, r, r, 1);
}

int ir(int r) {
  if(r != 15 && r != 13) return r+1;
  return r;
}

void incr(unsigned i) {
  for(; i < nmem; i++) {
    unsigned ins = mem[i].ins;
    if((ins&DCW) == DCW);
    else {
      unsigned n = ins&0xf000|ir(ins>>8&15)<<8|ir(ins>>4&15)<<4;
      if(ins&0x8000) ins = n|ir(ins&15);
      else ins = n|ins&15;
      mem[i].ins = ins;
    }
  }
}

int sltsltp(int r) {
  unsigned ins = 0x5001|r<<4|r<<8;
  int i = nmem-1;
  while(mem[i].ins == BLANK) i--;
  return mem[i].ins == ins && mem[i-1].ins == ins;
}

void dellast() {
  int i;
  for(i = nmem-1; mem[i].ins == BLANK; i--);
  for(int j = i; j < nmem; j++) mem[j] = mem[j+1];
  nmem--;
}

void slt(int r) {
  if(sltsltp(r)) {
    dellast();
  } else dins(5, r, r, 1);
}

void sltslt(int r) {
  if(sltsltp(r));
  else {
    dins(5, r, r, 1);
    dins(5, r, r, 1);
  }
}

void compileOp(int n, int o) {
  switch(o) {
  case ADD: op(n, 0); break;
  case SUB: op(n, 1); break;
  case AND: op(n, 2); break;
  case OR: op(n, 3); dins(12, n, n, n); break;
  case LT: op(n, 5); break;
  case GE: op(n, 5); dins(5, n, n, 1); break;
  case LE: inc(); op(n, 4); break;
  case GT: inc(); op(n, 4); dins(5, n, n, 1); break;
  case EQU: op(n, 1); dins(5, n, n, 1); break;
  case NEQ: op(n, 1); dins(5, n, n, 1); dins(5, n, n, 1); break;
  }
}

int prec(int o) {
  switch(o) {
  case MUL: case DIV: case MOD: return 1;
  case ADD: case SUB: return 2;
  case SHL: case SHR: return 3;
  case LT: case GT: case LE: case GE: return 4;
  case EQU: case NEQ: return 5;
  case AND: return 6;
  case XOR: return 7;
  case OR: return 8;
  case ANAN: return 9;
  case OROR: return 10;
  case TERN: case COL: return 11;
  case EQ: case MULEQ: case DIVEQ: case MODEQ:
  case ADDEQ: case SUBEQ: case SHLEQ: case SHREQ:
  case ANDEQ: case XOREQ: case OREQ: return 12;
  case COM: return 13;
  default: return 0;
  }
}

int findGlobal(int t) {
  for(int i = 0; i < nglobals; i++)
    if(globals[i] == t) return i;
  return -1;
}

void compileCom();

void store(int n, int st) {
  if((st&0x7000) != 0x6000) err("expected lvalue");
  dc(0, st&0xf0ff|0x1000|n<<8, 0);
}

int findLocal(int t) {
  for(int i = 0; i < nlocals; i++)
    if(locals[i] == t) return i;
  return -1;
}

void compilePost(int n) {
  unsigned o = nmem;
  int i;
  if(look >= NUM0) {
    unsigned v = nums[look-NUM0];
    if(v < 16) {
      dins(9, n, n, n);
      dins(0, n, n, v);
    } else {
      dins(6, n, 15, 1);
      dins(0, 15, 15, 1);
      dc(0, DCW, v);
    }
  } else if((i = findLocal(look)) != -1) {
    printf("AAAH: n: %d,%d\n", n, i);
    dins(6, n, 13, i);
  } else if((i = findGlobal(look)) != -1) {
    dins(6, n, 15, 1);
    dins(0, 15, 15, 1);
    dc(0, DCL, i);
    if(globalt[i] != FUN && globalt[i] != UNDEF) {
      dins(6, n, n, 0);
    }
  } else errf("unknown value %s", look);
  for(;;) {
    next();
    if(look == LB) {
      next();
      compileCom(n+1);
      op(n, 6);
      if(look != RB) err("expected ]");
    } else if(look == PP || look == MM) {
      incr(o);
      unsigned st = mem[nmem-1].ins;
      mem[nmem-1].ins -= 0x0100;
      dins(look == MM, n+2, n, 1);
      store(n+2, st);
    } else break;
  }
}

void compileUnary(int n) {
  while(look == ADD) next();
  if(look == SUB) {
    next(); compileUnary(n); dins(11, n, n, n); dins(1, n, n, 1);
  } else if(look == INV) {
    next(); compileUnary(n); dins(11, n, n, n);
  } else if(look == NOT) {
    next(); compileUnary(n); dins(5, n, n, 1);
  } else if(look == MUL) {
    next(); compileUnary(n); dins(6, n, n, 0);
  } else if(look == LP) {
    next(); compileCom(n);
    if(look != RP) err("expected )");
  } else if(look == PP || look == MM) {
    next(); compileUnary(n+1);
    unsigned st = mem[nmem-1].ins;
    mem[nmem-1].ins -= 0x0100;
    dins(look == MM, n, n, 1);
    store(n, st);
  } else compilePost(n);
}

void compileOper(int n, int p) {
  if(!p) { compileUnary(n); return; }
  compileOper(n, p-1);
  printf("on: %s\n", str(look));
  while(prec(look) == p) {
    int o = look; next();
    compileOper(n+1, p-1);
    compileOp(n, o);
  }
}

int label(const char *fmt, int n) {
  sprintf(idnext, fmt, n);
  labels[nlabels] = idnext;
  idnext += strlen(idnext)+1;
  return nlabels++;
}

void addLabel(int l) {
  if(mem[nmem-1].label) dc(l+1, BLANK, 0);
  else mem[nmem-1].label = l+1;
}

void compileLand(int n) {
  compileOper(n, 8);
  while(look == ANAN) {
    int a = label("AN%dA", nand), b = label("AN%dB", nand);
    nand++;
    slt(n);
    dins(14, 15, 15, n);
    dc(0, DCL, a);
    dc(0, DCL, b);
    unsigned o = nmem;
    next(); compileOper(n, 8);
    mem[o].label = a+1;
    sltslt(n);
    dc(b+1, BLANK, 0);
  }
}

void compileLor(int n) {
  compileLand(n);
  while(look == OROR) {
    int a = label("OR%dA", nor), b = label("OR%dB", nor);
    nor++;
    slt(n);
    dins(14, 15, 15, n);
    dc(0, DCL, b);
    dc(0, DCL, a);
    unsigned o = nmem;
    next(); compileLand(n);
    mem[o].label = a+1;
    sltslt(n);
    addLabel(b);
  }
}

void compileTern(int n) {
  compileLor(n);
  if(look == TERN) {
    int a = label("TR%dA", ntern), b = label("TR%dB", ntern);
    int c = label("TR%dC", ntern); ntern++;
    slt(n);
    dins(14, 15, 15, n);
    dc(0, DCL, a);
    dc(0, DCL, b);
    unsigned o = nmem;
    next();
    compileTern(n);
    mem[o].label = a+1;
    dc(0, 0x6ff0, 0);
    dc(0, DCL, c);
    if(look != COL) err("expected :"); next();
    o = nmem;
    compileTern(n);
    mem[o].label = b+1;
    dc(c+1, BLANK, 0);
  }
}

void compileAss(int n) {
  unsigned o = nmem;
  compileTern(n);
  if(look == EQ) {
    incr(o);
    int st = mem[nmem-1].ins; nmem--;
    next(); compileAss(n);
    store(n, st);
  } else if(look >= ADDEQ && look < EQ) {
    incr(o);
    int o = look-ADDEQ+ADD;
    int st = mem[nmem-1].ins;
    mem[nmem-1].ins -= 0x0100;
    next(); compileAss(n+1);
    compileOp(n, o);
    store(n, st);
  }
}

void compileCom(int n) {
  compileAss(n);
  while(look == COM) { next(); compileAss(n); }
}

void compileEx() {
  printf("EXPRESSION\n");
  compileCom(0);
  printf("DONE: %s\n",str(look));
}

void saveAs() {
  for(int i = 0; i < nmem; i++) {
    unsigned ins = mem[i].ins;
    if(mem[i].label) fprintf(out, "_%s", labels[mem[i].label-1]);
    fprintf(out, "\t");
    if(ins == DCW) fprintf(out, "DCW %d\n", mem[i].param);
    else if(ins == DCL) fprintf(out, "DCW _%s\n", labels[mem[i].param]);
    else if(ins == BLANK) fprintf(out, "\n");
    else {
      fprintf(out, "%.3s %c,%c,", "ADDSUBANDNORRORSLTLDWSTW"+(ins>>12&7)*3,
        (ins>>8&15)+'A', (ins>>4&15)+'A');
      if(ins&0x8000) fprintf(out, "%c\n", (ins&15)+'A');
      else fprintf(out, "#%d\n", ins&15);
    }
  }
  for(int i = 0; i < nglobals; i++) {
    if(globalt[i] >= BSS) {
      char *s = str(globals[i]);
      fprintf(out, "_%s\tDCW D_%s\n", s, s);
    }
  }
  for(int i = 0; i < nglobals; i++) {
    if(globalt[i] >= BSS) {
      char *s = str(globals[i]);
      fprintf(out, "D_%s\tRSW %d\n", s, globalt[i]-BSS);
    }
  }
}

void checkid(int t) {
  if(t >= NUM0 || t < ID0) errf("invalid identifier %s", t);
  if(findLocal(t) != -1) errf("%s is already defined", t);
}

void expect(int t) {
  printf("look: %s\n", str(look));
  if(look != t) errf("expected %s", t);
  next();
}

int evalOper(int p);

int evalUnary() {
  int n;
  switch(look) {
  case SUB: next(); return -evalUnary();
  case INV: next(); return ~evalUnary();
  case ADD: next(); return evalUnary();
  case LP: next(); n = evalOper(10); expect(RP); return n;
  }
  if(look >= NUM0) { n = nums[look-NUM0]; next(); return n; }
  if(look >= ID0) {
    n = findGlobal(look); next();
    if(globalt[n] < UNDEF) return consts[globalt[n]-CONST];
  }
  errf("failed to evaluate %s", look);
}

int evalOper(int p) {
  if(!p) return evalUnary();
  int a = evalOper(p-1);
  while(prec(look) == p) {
    int op = look; next();
    int b = evalOper(p-1);
    switch(op) {
    case ADD: a += b; break;
    case SUB: a -= b; break;
    case ANAN: a = a && b; break;
    case OROR: a = a || b; break;
    case AND: a &= b; break;
    case OR: a |= b; break;
    case XOR: a ^= b; break;
    case MUL: a *= b; break;
    case DIV: if(!b) err("division by zero"); a /= b; break;
    case MOD: if(!b) err("division by zero"); a %= b; break;
    case EQU: a = a == b; break;
    case NEQ: a = a != b; break;
    case LT: a = a < b; break;
    case GT: a = a > b; break;
    case LE: a = a <= b; break;
    case GE: a = a >= b; break;
    case SHR: a = a >> b; break;
    case SHL: a = a << b; break;
    }
  }
  return a;
}

int evalTern() {
  int n = evalOper(10);
  if(look == TERN) {
    next();
    int a = evalTern();
    expect(COL);
    int b = evalTern();
    return n ? a : b;
  }
  return n;
}

int eval() {
  return evalTern();
}

void compileStatement() {
  if(look == LC) {
    next();
    while(look != RC) {
      if(look == RC) break;
      compileStatement(); printf("[%s]\n", str(look));
    }
    next();
  } else {
    compileEx(); expect(SEMI);
  }
}

void compileFunction() {
  nmem = 0;
  nlocals = 0;
  while(look != RP) {
    checkid(look); locals[nlocals++] = look;
    if(nlocals >= 13) err("too many args in definition");
    next(); if(look == RP) break;
    expect(COM);
  }
  next();
  argc = nlocals;
  if(argc > 13) err("too many args in definition");
  compileStatement();
  fprintf(out, "_%s\tSUB N,N,#%d\n", str(id), nlocals);
  for(int i = 0; i < argc; i++)
    fprintf(out, "\tSTW %c,N,#%d\n", i+'A', i);
  fprintf(out, "\tSTW O,N,#%d\n", nlocals);
  saveAs();
  fprintf(out, "\tLDW O,N,#%d\n", nlocals);
  fprintf(out, "\tADD N,N,#%d\n", nlocals+1);
  fprintf(out, "\tADD P,O,#0\n");
  nlocals = 0;
}

void compileFile() {
  while(look != EOF) {
    next();
    checkid(look);
    id = look; next();
    int i = findGlobal(id);
    if(i != -1) {
      if(globalt[i] == UNDEF && look == LP) {
        globalt[i] = FUN;
        next(); compileFunction();
      } else errf("%s is already defined", id);
    }
    globals[i = nglobals++] = id;
    if(look == SEMI) globalt[i] = BSS+1;
    else if(look == LB) {
      next(); int n = eval();
      expect(RB); expect(COM);
      if(n <= 0) errf("invalid dimension for %s", id);
      globalt[i] = n+BSS;
    } else if(look == LP) {
      globalt[i] = FUN;
      next(); compileFunction();
    } else err("expected definition");
    id = 0;
  }
}

int main(int argc, char **args) {
  if(argc < 3) {
    printf("usage: %s <file1.t> [<file2.t> <file3.t> ...] <output.s>\n", args[0]);
    return 1;
  }
  //out = fopen(args[argc-1], "w");
  //if(!out) { printf("failed to open %s\n", args[argc-1], return 1; }
  out = stdout;
  for(int i = 1; i < argc-1; i++) {
    ln = 1;
    fp = fopen(args[i], "r");
    if(!fp) { printf("failed to open %s\n", args[i]); return 1; }
    compileFile();
    fclose(fp);
  }
  //fclose(out);
  return 0;
}
