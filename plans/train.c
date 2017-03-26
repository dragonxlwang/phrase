//////////////////////////////////////////////////////////////////
// Copyright (c) 2016 Xiaolong Wang All Rights Reserved.        //
// Code for ``Dual Clustered Maximal Entropy for Many Classes'' //
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

void Train(int argc, char* argv[]) {
  NumInit();
  VariableInit(argc, argv);  //  >>
  MainWork prepper = NULL, cleaner = NULL;
  ThreadTrain trainer = NULL;
  trainer = PlansThreadTrain;
  prepper = PlansPrep;
  if (prepper) prepper();
  if (trainer) {
    long int tid;
    // schedule thread jobs
    pthread_t* pt = (pthread_t*)malloc(V_THREAD_NUM * sizeof(pthread_t));  // >>
    for (tid = 0; tid < V_THREAD_NUM; tid++)
      pthread_create(&pt[tid], NULL, trainer, (void*)tid);
    for (tid = 0; tid < V_THREAD_NUM; tid++) pthread_join(pt[tid], NULL);
    free(pt);  // <<
  }
  if (cleaner) cleaner();
  RestSave(rest, w_embd, V, N, -1, V_MODEL_SAVE_PATH);  // save model
  VariableFree();                                       // <<
  LOG(1, "\nTraining finished. Took time %s\n",
      strclock(start_clock_t, clock(), V_THREAD_NUM));
  return;
}

int main(int argc, char* argv[]) {
  Train(argc, argv);
  return 0;
}
