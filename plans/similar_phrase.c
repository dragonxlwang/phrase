//////////////////////////////////////////////////////////////////
// Copyright (c) 2016 Xiaolong Wang All Rights Reserved.        //
// Code for ``Phrasal Latent Attachment and Negative Sampling'' //
//////////////////////////////////////////////////////////////////

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../plans/constants.c"
#include "../plans/plans.c"
#include "../plans/variables.c"
#include "../utils/util_misc.c"
#include "../utils/util_num.c"
#include "../utils/util_text.c"

void Load(int argc, char *argv[]) {
  NumInit();
  VariableInit(argc, argv);  //  >>
  if (fexists(V_MODEL_SAVE_PATH)) {
    int nn, vv;
    rests = RestLoad(V_MODEL_SAVE_PATH, &w_embd, &nn, &vv);
    if (N != nn) {
      LOGC(0, 'r', 'k', "[MODEL]: overwrite N from %d to %d\n", N, nn);
      N = nn;
    }
    if (V != vv) {
      LOGC(0, 'r', 'k', "[MODEL]: overwrite V from %d to %d\n", V, vv);
      V = vv;
    }
  } else {
    LOGC(0, 'r', 'k', "Model file %s does not exist\n", V_MODEL_SAVE_PATH);
  }
}

void SortAndPrint(int k) {
  pair *pairs = RestSort(rests, k);
  int i, j;
  for (i = 0; i < k; i++) {
    j = pairs[i].key;
    printf("[%d]: %-100s %lf\n", i, rests->id2table[j], rests->id2cnum[j]);
  }
}

int main(int argc, char *argv[]) {
  Load(argc, argv);
  SortAndPrint(100);
  return 0;
}
