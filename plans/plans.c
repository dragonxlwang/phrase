/*
 * PLANS: Phrasal Latent Allocation with Negative Sampling
 */
#ifndef PLANS
#define PLANS

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
#include "neg_samp.c"
#include "phrase.c"
#include "restuarant.c"
#include "variables.c"

int sid_plans_ppb_lock = 0;
void PlansThreadPrintProgBar(int dbg_lvl, int tid, real p) {
  if (sid_plans_ppb_lock) return;
  sid_plans_ppb_lock = 1;

  char *str = malloc(0x1000);

  int i, bar_len = 10;
  clock_t cur_clock_t = clock();
  real pct = p * 100;
  int bar = p * bar_len;
  double st = (double)(cur_clock_t - start_clock_t) / CLOCKS_PER_SEC;
  char *ht = strtime(st / thread_num);
  sprintfc(str, 'y', 'k', "[%7.4lf%%]: ", pct);             // percentage
  for (i = 0; i < bar; i++) saprintfc(str, 'r', 'k', "+");  // bar: past
  saprintfc(str, 'w', 'k', "~");                            // bar: current
  for (i = bar + 1; i < bar_len; i++)                       //
    saprintfc(str, 'c', 'k', "=");                          // bar: left
  saprintf(str, " ");                                       //
  saprintfc(str, 'g', 'k', "(tid=%02d)", tid);              // tid
  saprintf(str, " ");                                       //
  saprintf(str, "TIME:%.2e/%s ", st, ht);                   // time
  saprintf(str, "GDSS:%.4e ", gd_ss);                       // gdss
  free(ht);
#ifdef DEBUG
  real scr = NumVecNorm(model->scr, model->v * model->n);
  real ss = NumMatMaxRowNorm(model->scr, model->v, model->n);
  real tar = NumVecNorm(model->tar, model->v * model->n);
  real tt = NumMatMaxRowNorm(model->tar, model->v, model->n);
  saprintf(str, "SCR:%.2e=%.2e*", scr, scr / ss);  // scr
  saprintfc(str, 'r', 'k', "%.2e", ss);            // ss
  saprintf(str, " ");                              //
  saprintf(str, "TAR=%.2e=%.2e*", tar, tar / tt);  // tar
  saprintfc(str, 'r', 'k', "%.2e", tt);            // tt
  saprintf(str, " ");                              //
#endif
  return str;
}

real PlansEvalLL(real *scr_embd, int wid, int is_posi, real d) {
  return NumLogSigmoid((is_posi ? 1 : -1) *
                       NumVecDot(scr_embd, w_embd + wid * N, N)) /
         d;
}

void PlansOptmLL(real *scr_embd, int wid, int is_posi, real d) {
  real *tar_embd = w_embd + wid * N;
  real s = gd_ss / d * (is_posi == 1 ? 1 : 0) -
           NumSigmoid(NumVecDot(scr_embd, tar_embd, N));
  NumVecAddCVec(scr_embd, tar_embd, s, N);
  NumVecAddCVec(tar_embd, scr_embd, s, N);
  return;
}

void PlansSample(int *ids, int l, int *neg_lst, int neg_num, int pos,
                 unsigned long *rs, int *beg_ptr, int *end_ptr) {
  int t = 0, u = 0, q, beg, end, i, j;
  real f, d;
  char str[0x1000];
  int beg_lst[PUP * PUP], end_lst[PUP * PUP], loc_lst[PUP * PUP],
      cnum_lst[PUP * PUP];
  real prob_lst[PUP * PUP], *scr_embd;
  for (beg = LARGER(pos - P + 1, 0); beg <= pos; beg++)    // iter beg
    for (end = pos + 1; end < SMALLER(beg + P, l); end++)  // iter end
      if (beg > 0 || end < l) {                            // context exists
        beg_lst[t] = beg;                                  //
        end_lst[t] = end;                                  //
        PhraseWid2Str(ids, beg, end, vcb, str);            // phrase str
        loc_lst[t] = RestLocate(rest, str);                // find in rest
        cnum_lst[t] = RestId2Cnum(rest, loc_lst[t]);       // customer number
        if (loc_lst[t] == -1) u++;
        t++;
      }
  for (i = 0; i < t; i++) {
    scr_embd = RestId2Embd(rest, loc_lst[i]);  // phrase embedding
    beg = beg_lst[i];
    end = end_lst[i];
    f = log(loc_lst[i] == -1 ? alpha / u : cnum_lst[i]);         // prior
    d = (beg - LARGER(beg - C, 0) + SMALLER(end + C, l) - end);  // pos discount
    for (j = LARGER(beg - C, 0); j < beg; j++)                   // left
      f += PlansEvalLL(scr_embd, ids[j], 1, d);                  //
    for (j = end; j < SMALLER(end + C, l); j++)                  // right
      f += PlansEvalLL(scr_embd, ids[j], 1, d);                  //
    for (j = 0; j < neg_num; j++)                                // neg samples
      f += PlansEvalLL(scr_embd, neg_lst[j], 0, 1);              //
    prob_lst[i] = f / temp;                                      // annealing
  }
  NumSoftMax(prob_lst, 1, t);
  q = NumMultinomialSample(prob_lst, t, rs);
  *beg_ptr = beg_lst[q];
  *end_ptr = end_lst[q];
  return;
}

void PlansUpdate(int *ids, int l, int *neg_lst, int neg_num, int beg, int end) {
  int i, j;
  char str[0x1000];
  real *scr_embd, d;
  PhraseWid2Str(ids, beg, end, vcb, str);  // phrase str
  i = RestLocateAndAdd(rest, str);
  scr_embd = RestId2Embd(rest, i);
  d = (beg - LARGER(beg - C, 0) + SMALLER(end + C, l) - end);  // pos discount
  for (j = LARGER(beg - C, 0); j < beg; j++)                   // left
    PlansOptmLL(scr_embd, ids[j], 1, d);                       //
  for (j = end; j < SMALLER(end + C, l); j++)                  // right
    PlansOptmLL(scr_embd, ids[j], 1, d);                       //
  for (j = 0; j < neg_num; j++)                                // neg samples
    PlansOptmLL(scr_embd, neg_lst[j], 0, 1);                   //
  return;
}

void PlansRun(int *ids, int l, unsigned long *rs) {
  int i, j, neg_lst[NUP], beg, end;
  for (i = 0; i < l; i++) {
    for (j = 0; j < ns->neg_num; j++) neg_lst[j] = NsSample(ns, rs);
    PlansSample(ids, l, neg_lst, ns->neg_num, i, rs, &beg, &end);
    PlansUpdate(ids, l, neg_lst, ns->neg_num, beg, end);
  }
  return;
}

void *PlansThreadTrain(void *arg) {
  int tid = (long)arg;
  FILE *fin = fopen(V_TEXT_FILE_PATH, "rb");
  if (!fin) {
    LOG(0, "Error!\n");
    exit(1);
  }
  fseek(fin, 0, SEEK_END);
  long int fsz = ftell(fin);
  long int fbeg = fsz / V_THREAD_NUM * tid;
  long int fend = fsz / V_THREAD_NUM * (tid + 1);
  long int fpos = fbeg;
  int wids[SUP], wnum;
  real p = 0;
  int iter_num = 0;
  fseek(fin, fbeg, SEEK_SET);  // training
  ///////////////////////////////////////////////////////////////////////////
  int i = 0;
  unsigned long rs = tid;
  while (iter_num < V_ITER_NUM) {
    wnum = TextReadSent(fin, vcb, wids, V_TEXT_LOWER, V_TEXT_RM_TRAIL_PUNC, 1);
    fpos = ftell(fin);
    if (wnum > 1) PlansRun(wids, wnum, &rs);
    if (i++ >= 10000) {
      i = 0;
      progress[tid] = iter_num + (double)(fpos - fbeg) / (fend - fbeg);  // prog
      p = GetTrainProgress(progress, V_THREAD_NUM, V_ITER_NUM);          //
      gd_ss = V_INIT_GRAD_DESCENT_STEP_SIZE * (1 - p);                   // gdss
      PlansThreadPrintProgBar(2, tid, p);                                // info
    }
    if (feof(fin) || fpos >= fend) {
      fseek(fin, fbeg, SEEK_SET);
      if (V_CACHE_INTERMEDIATE_MODEL &&
          iter_num % V_CACHE_INTERMEDIATE_MODEL == 0)
        ModelSave(model, iter_num, V_MODEL_SAVE_PATH);
      iter_num++;
    }
  }
  ///////////////////////////////////////////////////////////////////////////
  fclose(fin);
  pthread_exit(NULL);
  return 0;
}

#endif /* ifndef PLANS */
