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

#define CUR_REST ((&(rests[_RID])))

FILE *_log_file_handle;
pthread_mutex_t print_lock;
double _cur_pct = 0;
void PlansThreadPrintProgBar(int dbg_lvl, int tid, real p) {
  if (pthread_mutex_trylock(&print_lock) != 0) return;
  char str[0x1000], lstr[0x1000];
  int i, bar_len = 10, _to_log = 0;
  clock_t cur_clock_t = clock();
  real pct = p * 100;
  if (_cur_pct + 0.01 < pct) {
    _to_log = 1;
    _cur_pct = pct;
  }
  int bar = p * bar_len;
  double st = (double)(cur_clock_t - start_clock_t) / CLOCKS_PER_SEC;
  char *ht = strtime(st / V_THREAD_NUM);
  sprintfc(str, 'y', 'k', "[%7.4lf%%]: ", pct);             // percentage
  if (_to_log) sprintf(lstr, "%7.4lf ", pct);               //
  for (i = 0; i < bar; i++) saprintfc(str, 'r', 'k', "+");  // bar: past
  saprintfc(str, 'w', 'k', "~");                            // bar: current
  for (i = bar + 1; i < bar_len; i++)                       //
    saprintfc(str, 'c', 'k', "=");                          // bar: left
  saprintf(str, " ");                                       //
  saprintfc(str, 'g', 'k', "(tid=%02d)", tid);              // tid
  if (_to_log) saprintf(lstr, "%02d ", tid);                //
  saprintf(str, " ");                                       //
  saprintf(str, "TIME:%.2e/%s ", st, ht);                   // time
  if (_to_log) saprintf(lstr, "%.2e ", st);                 //
  saprintf(str, "GDSS:%.4e ", gd_ss);                       // gdss
  if (_to_log) saprintf(lstr, "%.4e ", gd_ss);              //
  free(ht);
  double w_embd_total_norm = NumVecNorm(w_embd, V * N);
  double w_embd_avg_norm = w_embd_total_norm / (V + 1e-6);
  saprintfc(str, 'g', 'k', "W_EMBD: %.4e=%.2e/%.2e ", w_embd_avg_norm,
            w_embd_total_norm,
            (double)V);  // w_embd
  if (_to_log)
    saprintf(lstr, "%.4e %.2e %.2e ", w_embd_avg_norm, w_embd_total_norm,
             (double)V);
  double rest_total_norm = REST_NORM(CUR_REST);
  double rest_size = CUR_REST->size;
  double rest_avg_norm = rest_total_norm / (rest_size + 1e-6);
  char cc = (_RID == 0 ? 'c' : 'r');
  saprintfc(str, cc, 'k', "REST[%d]: %.4e=%.2e/%.2e ", _RID, rest_avg_norm,
            rest_total_norm,
            rest_size);  // w_embd
  if (_to_log)
    saprintf(lstr, "%.4e %.2e %.2e ", rest_avg_norm, rest_total_norm,
             rest_size);
  double cnum_max = CUR_REST->id2cnum[0];
  double cnum_mean = NumVecMean(CUR_REST->id2cnum, 1000000);
  saprintfc(str, 'y', 'k', "cnum:%.2e,%.4lf ", cnum_max, cnum_mean);
  if (_to_log) saprintf(lstr, "%.2e %.4lf ", cnum_max, cnum_mean);
  double missing_ratio = _rest_add_new / _rest_add_all;
  saprintfc(str, 'b', 'k', "missing:%.2e ", missing_ratio);
  if (_to_log) saprintf(lstr, "%.2e ", missing_ratio);
  saprintfc(str, 'g', 'k', "reduce:%d ", _REDUCE_CNT);
  if (_to_log) saprintf(lstr, "%d ", _REDUCE_CNT);
  double single_word_ratio =
      (double)_single_word_size / (double)_total_phrase_size;
  saprintfc(str, 'g', 'k', "single:%.4e=%.2e/%.2e ", single_word_ratio,
            (double)_single_word_size, (double)_total_phrase_size);
  if (_to_log)
    saprintf(lstr, "%.4e %.2e %.2e ", single_word_ratio,
             (double)_single_word_size, (double)_total_phrase_size);
  LOGCLR(dbg_lvl);
  LOG(dbg_lvl, "%s", str);
  if (_to_log) {
    fprintf(_log_file_handle, "%s\n", lstr);
    fflush(_log_file_handle);
  }
  /* LOG(dbg_lvl, "\n"); */
  pthread_mutex_unlock(&print_lock);
}

real PlansEvalLL(real *scr_embd, int wid, int is_positive, real d) {
  real *tar_embd = w_embd + wid * N;
  real x = NumVecDot(scr_embd, tar_embd, N);
  x *= (is_positive ? 1.0 : -1.0);
  return NumLogSigmoid(x) / d;
}

void PlansOptmLL(real *scr_embd, int wid, int is_positive, real d) {
  real *tar_embd = w_embd + wid * N;
  real x = NumVecDot(scr_embd, tar_embd, N);
  x = (is_positive ? 1.0 : 0.0) - NumSigmoid(x);
  x *= gd_ss / d;
  NumVecAddCVec(scr_embd, tar_embd, x, N);
  if (V_MODEL_PROJ_BALL_NORM > 0)
    NumVecProjUnitBall(scr_embd, V_MODEL_PROJ_BALL_NORM, N);
  NumVecAddCVec(tar_embd, scr_embd, x, N);
  if (V_MODEL_PROJ_BALL_NORM > 0)
    NumVecProjUnitBall(tar_embd, V_MODEL_PROJ_BALL_NORM, N);
  return;
}

real PlansGetDiscount(int beg, int end, int l, int neg_num) {
  // beg - C ... [ beg, end] ... end + C
  real d = beg - LARGER(beg - C, 0) + SMALLER(end + C, l - 1) - end;
  return d;
}

void PlansSample(int *ids, int l, int *neg_lst, int neg_num, int pos,
                 unsigned long *rs, int *beg_ptr, int *end_ptr) {
  int t = 0, u = 0, q, beg, end, i, j;
  real f, d;
  char str[0x1000];
  int beg_lst[PUP * PUP], end_lst[PUP * PUP], loc_lst[PUP * PUP],
      cnum_lst[PUP * PUP];
  real prob_lst[PUP * PUP], *scr_embd;
  for (beg = LARGER(pos - P, 0); beg <= pos; beg++)      // iter beg
    for (end = pos; end < SMALLER(beg + P, l); end++) {  // iter end
      if (beg == 0 && end == l - 1) continue;            //
      beg_lst[t] = beg;                                  // select [beg, end]
      end_lst[t] = end;                                  //
      PHRASE_WID2STR(ids, beg, end, vcb, str);           // phrase str
      loc_lst[t] = REST_LOCATE(CUR_REST, str);           // find in rest
      cnum_lst[t] = REST_ID2CNUM(CUR_REST, loc_lst[t]);  // customer number
      if (loc_lst[t] == -1) u++;                         // unseen words
      t++;
    }
  for (i = 0; i < t; i++) {
    scr_embd = REST_ID2EMBD(CUR_REST, loc_lst[i]);        // phrase embedding
    beg = beg_lst[i];                                     //
    end = end_lst[i];                                     //
    f = log(loc_lst[i] == -1 ? alpha / u : cnum_lst[i]);  // prior
    d = PlansGetDiscount(beg, end, l, neg_num);           // discount
    for (j = LARGER(beg - C, 0); j <= beg - 1; j++)       // left
      f += PlansEvalLL(scr_embd, ids[j], 1, d);           //
    for (j = end + 1; j <= SMALLER(end + C, l - 1); j++)  // right
      f += PlansEvalLL(scr_embd, ids[j], 1, d);           //
    for (j = 0; j < neg_num; j++)                         // neg samples
      f += PlansEvalLL(scr_embd, neg_lst[j], 0, 1);       //
    prob_lst[i] = f * inv_temp;                           // annealing
  }
  NumSoftMax(prob_lst, 1, t);
  q = NumMultinomialSample(prob_lst, t, rs);
  *beg_ptr = beg_lst[q];
  *end_ptr = end_lst[q];
  return;
}

void PlansUpdate(int *ids, int l, int *neg_lst, int neg_num, int beg, int end) {
  int i, j;
  char str[MAX_PHRASE_LEN];
  real *scr_embd, d;
  PHRASE_WID2STR(ids, beg, end, vcb, str);  // phrase str
  i = REST_LOCATE_AND_ADD(CUR_REST, str);
  scr_embd = REST_ID2EMBD(CUR_REST, i);
  d = PlansGetDiscount(beg, end, l, neg_num);
  for (j = LARGER(beg - C, 0); j <= beg - 1; j++)       // left
    PlansOptmLL(scr_embd, ids[j], 1, d);                //
  for (j = end + 1; j <= SMALLER(end + C, l - 1); j++)  // right
    PlansOptmLL(scr_embd, ids[j], 1, d);                //
  for (j = 0; j < neg_num; j++)                         // neg samples
    PlansOptmLL(scr_embd, neg_lst[j], 0, 1);            //
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
      inv_temp = exp(p * log(V_FINAL_INV_TEMP));                         // temp
      PlansThreadPrintProgBar(2, tid, p);                                // info
    }
    if (feof(fin) || fpos >= fend) {
      fseek(fin, fbeg, SEEK_SET);
      if (V_CACHE_INTERMEDIATE_MODEL &&
          iter_num % V_CACHE_INTERMEDIATE_MODEL == 0)
        RestSave(rests, w_embd, V, N, iter_num, V_MODEL_SAVE_PATH);
      iter_num++;
    }
  }
  ///////////////////////////////////////////////////////////////////////////
  fclose(fin);
  pthread_exit(NULL);
  return 0;
}

void PlansPrep() {
  if (pthread_mutex_init(&print_lock, NULL) != 0) {
    LOG(0, "[PLANS] Mutex init failed\n");
    return;
  }
  // add vocabulary to restaurant as priors
  LOG(0, "[PLANS] Adding vocabulary to restaurants\n");
  int i = 0, j, t;
  real prior = 2 * V;
  Restaurant *rr;
  char *ss;
  for (t = 0; t < 2; t++) {
    rr = &(rests[t]);
    for (i = 0; i < vcb->size; i++) {
      ss = VocabGetWord(vcb, i);
      strncpy(rr->id2table[i], ss, MAX_PHRASE_LEN);
      rr->id2cnum[i] = ns->prob[i] * prior;
      NumCopyVec(rr->id2embd[i], rr->default_embd, rr->embd_dim);
      REST_BKDR_LINK_NO_LOCK(ss, i, rr);
    }
    rr->size = vcb->size;
    rr->customer_cnt = 0;
  }
  char *log_file_path = sformat("%s.log", V_MODEL_SAVE_PATH);
  LOG(0, "log file path: %s\n", log_file_path);
  _log_file_handle = fopen(log_file_path, "wb");
  free(log_file_path);
  LOG(0, "[PLANS] Finished\n");
}

void PlansClean() {
  fclose(_log_file_handle);
  return;
}

#endif /* ifndef PLANS */
