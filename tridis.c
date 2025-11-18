#include <stdio.h>
#include <stdlib.h>

#define W unsigned short

const char *istr = "ADDSUBANDNORRORSLTLDWSTW";

int main(int argc, char **args) {
  FILE *fp;
  for(int i = 1; i < argc; i++) {
    fp = fopen(args[i], "rb");
    if(!fp) { printf("failed to open %s\n", args[i]); return 1; }
    W pc = 0;
    W ins;
    while(fread(&ins, 2, 1, fp) == 1) {
      W a = ins>>8&15, b = ins>>4&15, c = ins&15;
      W n = ins>>12&7;
      printf("%.4X %.4X %.3s %c,%c,", pc++, ins, istr+n*3, a+'A', b+'A');
      if(ins&0x8000) printf("%c\n", 'A'+c);
      else printf("#%d\n", c);
    }
    fclose(fp);
  }
  return 0;
}
