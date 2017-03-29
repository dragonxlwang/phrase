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

void FindPhraseByPhrase(int k, int l) {
  Restaurant *r = rests;
  int i, j;
  real *sim = NumNewHugeVec(r->size);
  LOG(0, "\nComputing similarity");
  for (i = 0; i < r->size; i++) {
    sim[i] = NumVecDot(r->id2embd[i], r->id2embd[k], r->embd_dim);
    sim[i] /= NumVecNorm(r->id2embd[i], r->embd_dim) *
              NumVecNorm(r->id2embd[k], r->embd_dim);
  }
  LOGCLR(0);
  LOG(0, "Sorting");
  pair *p = sorted(sim, r->size, 1);
  int *wids = NumNewIntVec(l);
  int *pids = NumNewIntVec(l);
  int wlen = 0, plen = 0;
  i = 0;
  LOGCLR(0);
  LOG(0, "Sorting Finished");
  while (wlen < l || plen < l) {
    j = p[i++].key;
    if (j != k) {
      if (PHRASE_SINGLE_WORD(r->id2table[j])) {
        if (wlen < l) wids[wlen++] = j;
      } else {
        if (plen < l) pids[plen++] = j;
      }
    }
    if (i % 1000 == 0) {
      LOGCLR(0);
      LOG(0, "Adding %d", i);
    }
  }
  LOGCLR(0);
  LOG(0, "PHRASE: [%d] %s\n", k, r->id2table[k]);
  for (i = 0; i < l; i++) {
    LOG(0, "%03d: [%10d]:%10.4e %-50s [%10d]:%10.4e %-80s\n", i, wids[i],
        sim[wids[i]], r->id2table[wids[i]], pids[i], sim[pids[i]],
        r->id2table[pids[i]]);
  }
  LOG(0, "\n");
  free(pids);
  free(wids);
  free(sim);
  free(p);
}

void SortAndPrint(int k) {
  size_t line_size = 0x1000;
  char *line = malloc(line_size);
  LOG(0, "sorting\n");
  if (k == -1) k = rests[0].size;
  pair *pairs = RestSort(rests, k);
  LOG(0, "finished\n");
  int i, j, x;
  int l = 0;
  for (i = 0; i < k; i++) {
    j = pairs[i].key;
    if (PHRASE_SINGLE_WORD(rests->id2table[j])) continue;
    printf("[%d]: %-100s %lf\n", j, rests->id2table[j], rests->id2cnum[j]);
    if (++l % 60 == 0) {
      while (1) {
        getline(&line, &line_size, stdin);
        if (line[0] == '\n') break;
        sscanf(line, "%d\n", &x);
        FindPhraseByPhrase(x, 60);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  Load(argc, argv);
  SortAndPrint(-1);
  return 0;
}
