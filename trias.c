#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W unsigned short

W ln, org;
FILE *fp;
char lbuf[100];
char *line[6];
int len;
char lbl[200][8];
int lblv[200];
int nlbl = 0;
const char *istr = "ADDSUBANDNORRORSLTLDWSTW";
W mem[16384];
W nmem = 0;
char ahc = 0;

void err(const char *e) {
  printf("%d: %s\n", ln, e);
  exit(1);
}

void syntax() {
  err("syntax error");
}

void getNext(char *s) {
  char c;
  char *b = s;
  for(;;) {
    c = fgetc(fp);
    if(c == 10 || c == EOF) { if(b != s) ungetc(c, fp); break; }
    else if(c <= 32 || c == ',') { if(b != s) break; }
    else *s++ = c;
  }
  *s = 0;
}

int findLbl(char *s) {
  for(int i = 0; i < nlbl; i++)
    if(!strcmp(lbl[i], s)) return i;
  return -1;
}

void getLine() {
  len = 0;
  int l = -1;
  char *s;
  char *b = lbuf;
  for(;;) {
    line[len] = b;
    getNext(b);
    if(!*b) break;
    if(s = strchr(b, ';')) { *s = 0; l = len; if(*b) l++; }
    len++;
    b += strlen(b)+1;
  }
  if(l != -1) len = l;
  ln++;
}

W vaal(char *s) {
  if(!strcmp(s, "$")) return org;
  int i = findLbl(s);
  if(i != -1) return lblv[i];
  W n = 0;
  while(*s) {
    if(*s >= '0' && *s <= '9') n = n*10 + *s++ - '0';
    else err("unknown value");
  }
  return n;
}

void head(char *buf, char *s) {
  strcpy(buf, s);
  s = strchr(buf, '-');
  if(!s) s = strchr(buf, '+');
  if(s) *s = 0;
}

W val(char *s) {
  char buf[256];
  W a;
  if(*s == '-') a = 0;
  else { head(buf, s); a = vaal(buf); s += strlen(buf); }
  while(*s) {
    char c = *s++;
    head(buf, s);
    W b = vaal(buf);
    s += strlen(buf);
    switch(c) {
    case '+': a += b; break;
    case '-': a -= b; break;
    default: syntax();
    }
  }
  return a;
}

void ac(int a) {
  if(len != a+1) err("wrong number of args");
}

void pass1() {
  ln = org = 0;
  while(!feof(fp)) {
    char **l = line;
    getLine();
    if((len&1) == 1) {
      if(strlen(l[0]) > 7) l[0][7] = 0;
      if(findLbl(l[0]) != -1) err("label already defined");
      lblv[nlbl] = org;
      strcpy(lbl[nlbl++], l[0]);
      l++; len--;
    }
    if(!len);
    else if(!strcmp(l[0], "ORG")) { ac(1); org = val(l[1]); }
    else if(!strcmp(l[0], "RES")) { ac(1); org += val(l[1]); }
    else if(!strcmp(l[0], "DCW")) { ac(1); org++; }
    else { ac(3); org++; }
  }
}

W ins(char *s) {
  if(strlen(s) == 3)
    for(int i = 0; i < 8; i++)
      if(s[0] == istr[i*3] && s[1] == istr[i*3+1] && s[2] == istr[i*3+2]) return i;
  err("unknown instruction");
}

W r(char *s) {
  if(s[1] == 0 && *s >= 'A' && *s <= 'P') return *s-'A';
  err("invalid register");
}

void pass2() {
  ln = org = 0;
  while(!feof(fp)) {
    char **l = line;
    getLine();
    if((len&1) == 1) { l++; len--; }
    if(!len);
    else if(!strcmp(l[0], "ORG")) org = val(l[1]);
    else if(!strcmp(l[0], "RES")) org += val(l[1]);
    else if(!strcmp(l[0], "DCW")) { org++; mem[nmem++] = val(l[1]); }
    else {
      org++;
      W n = ins(l[0])<<12|r(l[1])<<8|r(l[2])<<4;
      if(*l[3] == '#') {
        W i = val(l[3]+1);
        if(i >= 16) err("value out of range");
        n |= i;
      } else n = n|0x8000|r(l[3]);
      mem[nmem++] = n;
    }
  }
}

int main(int argc, char **args) {
  if(argc != 3) { printf("usage: %s <file.s> <out.bin>\n"); return 1; }
  fp = fopen(args[1], "r");
  if(!strcmp(args[1], args[2])) { printf("same input and output file\n"); return 1; }
  if(!fp) { printf("failed to open %s\n", args[1]); return 1; }
  pass1(fp);
  fclose(fp);
  fp = fopen(args[1], "r");
  pass2(fp);
  fclose(fp);
  fp = fopen(args[2], "wb");
  if(!fp) { printf("failed to open %s\n", args[2]); return 1; }
  fwrite(mem, 2, nmem, fp);
  fclose(fp);
  printf("assembled %d words\n", nmem);
  return 0;
}
