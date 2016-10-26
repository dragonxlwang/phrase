#ifndef PHRASE
#define PHRASE

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../utils/util_misc.c"
#include "../utils/util_num.c"
#include "../utils/util_text.c"

void PhraseWid2Str(int *idx, int beg, int end, Vocabulary *vcb, char *s) {
  s[0] = 0;
  int i;
  for (i = beg; i < end; i++) {
    if (i != beg) saprintf(s, "=||=");
    saprintf(s, VocabGetWord(vcb, idx[i]));
  }
  return;
}

#endif /* ifndef PHRASE */
