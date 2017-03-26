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

#define PHRASE_WID2STR(IDS, BEG, END, VCB, STR)                \
  ({                                                           \
    int _i;                                                    \
    for (_i = BEG; _i <= END; _i++) {                          \
      if (_i == BEG) {                                         \
        strcpy(STR, VocabGetWord(VCB, IDS[_i]));               \
      } else {                                                 \
        strcat(STR + strlen(STR), "=||=");                     \
        strcat(STR + strlen(STR), VocabGetWord(VCB, IDS[_i])); \
      }                                                        \
      if (strlen(STR) + 2 * WUP > MAX_PHRASE_LEN) break;       \
    }                                                          \
  })
#endif /* ifndef PHRASE */
