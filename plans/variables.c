#ifndef VARIABLES
#define VARIABLES

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
#include "constants.c"
#include "neg_samp.c"
#include "phrase.c"
#include "restuarant.c"

// ---------------------------- Config Variables ------------------------------
char *V_MODEL_DECOR_FILE_PATH = NULL;
char *V_TEXT_FILE_PATH = "~/local/data/gigaword/giga_nyt.txt";  // "text8/text8"
char *V_VOCAB_FILE_PATH = NULL;  // don't set it if can be inferred from above
char *V_MODEL_SAVE_PATH = NULL;

int V_THREAD_NUM = 40;
int V_ITER_NUM = 10;

// Initial grad descent step size
real V_INIT_GRAD_DESCENT_STEP_SIZE = 1e-4;
// final inverse temperature, log-linear monotonically increasing from 1
real V_FINAL_INV_TEMP = 5;
// shrink rate for customer number
real V_REST_SHRINK_RATE = 0.99;
// interval size between shrink in restaurant
real V_REST_INTERVAL_SIZE = 0.5e6;
// max number of tables in restaurant
real V_REST_MAX_TABLE_SIZE = 1.5e6;
// Model Shrink: l-2 regularization:
real V_L2_REGULARIZATION_WEIGHT = -1;  // 1e-3;
// Model Shrink: if proj model to ball with specified norm, -1 if not proj
real V_MODEL_PROJ_BALL_NORM = -1;
// Vocab loading option: cut-off high frequent (stop) words
int V_VOCAB_HIGH_FREQ_CUTOFF = 80;
// if convert uppercase to lowercase
int V_TEXT_LOWER = 1;
// if remove trailing punctuation
int V_TEXT_RM_TRAIL_PUNC = 1;
// if cache model per iteration
int V_CACHE_INTERMEDIATE_MODEL = 1;
// if overwrite vocab file
int V_VOCAB_OVERWRITE = 0;
// if load model from file instead of random initiailization
int V_MODEL_LOAD = 0;
int N = 100;       // embedding dimension
int V = 0.1e6;     // vocabulary size cap, 100K, set to -1 if no limit
int C = 5;         // context length
int P = 4;         // phrase max length
real alpha = 0.1;  // CRP prior

// -------------------------------- Model -------------------------------------
Vocabulary *vcb;      // vocabulary
real *w_embd;         // word embedding (target only)
Restaurant *rests;    // restaurants (two, one is the backup)
NegativeSampler *ns;  // negative sampler
// ---------------------------- global variables ------------------------------
real gd_ss;             // grad descend step size
real inv_temp;          // inverse of temperature (monotonically increasing)
real *progress;         // thread progress
clock_t start_clock_t;  // start time for model training
// ------------------------ global private variables --------------------------
real model_init_amp = 1e-4;  // model init: small value for numeric stability
typedef void *(*ThreadTrain)(void *);
typedef void (*MainWork)();

real GetTrainProgress(real *progress, int thread_num, int iter_num) {
  real p = NumSumVec(progress, thread_num);
  p /= (thread_num * iter_num);
  return p;
}

void VariableInit(int argc, char **argv) {
  int i;
  char c = 'w';

  i = getoptpos("V_MODEL_DECOR_FILE_PATH", argc, argv);
  if (i != -1) {
    V_MODEL_DECOR_FILE_PATH = sclone(argv[i + 1]);
    LOGC(1, 'g', 'k', "%s",
         sformat("== Config: %s ==\n", V_MODEL_DECOR_FILE_PATH));
  } else
    LOGC(1, 'g', 'k', "== Config ==\n");

  i = getoptpos("V_TEXT_FILE_PATH", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_TEXT_FILE_PATH = sclone(argv[i + 1]);
  V_TEXT_FILE_PATH = FilePathExpand(V_TEXT_FILE_PATH);
  LOGC(1, c, 'k', "Input File -------------------------------------- : %s\n",
       V_TEXT_FILE_PATH);

  i = getoptpos("V_TEXT_LOWER", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_TEXT_LOWER = atoi(argv[i + 1]);
  i = getoptpos("V_TEXT_RM_TRAIL_PUNC", argc, argv);
  if (c == 'w') c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_TEXT_RM_TRAIL_PUNC = atoi(argv[i + 1]);
  i = getoptpos("V_VOCAB_FILE_PATH", argc, argv);
  if (c == 'w') c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_VOCAB_FILE_PATH = sclone(argv[i + 1]);
  if (!V_VOCAB_FILE_PATH)
    V_VOCAB_FILE_PATH = FilePathSubExtension(
        V_TEXT_FILE_PATH,
        sformat("l%dr%d.vcb", V_TEXT_LOWER, V_TEXT_RM_TRAIL_PUNC));
  else
    V_VOCAB_FILE_PATH = FilePathExpand(V_VOCAB_FILE_PATH);
  LOGC(1, c, 'k', "Vocab File -------------------------------------- : %s\n",
       V_VOCAB_FILE_PATH);

  i = getoptpos("V_MODEL_SAVE_PATH", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_MODEL_SAVE_PATH = sclone(argv[i + 1]);
  if (!V_MODEL_SAVE_PATH) {
    if (V_MODEL_DECOR_FILE_PATH) {
      V_MODEL_SAVE_PATH = FilePathSubExtension(
          V_TEXT_FILE_PATH, sformat("%s.mdl", V_MODEL_DECOR_FILE_PATH));
      c = 'r';
    } else
      V_MODEL_SAVE_PATH = FilePathSubExtension(V_TEXT_FILE_PATH, "mdl");
  } else
    V_MODEL_SAVE_PATH = FilePathExpand(V_MODEL_SAVE_PATH);
  LOGC(1, c, 'k', "Model File -------------------------------------- : %s\n",
       V_MODEL_SAVE_PATH);

  i = getoptpos("V_THREAD_NUM", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_THREAD_NUM = atoi(argv[i + 1]);
  LOGC(1, c, 'k', "Thread Num -------------------------------------- : %d\n",
       V_THREAD_NUM);

  i = getoptpos("V_ITER_NUM", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_ITER_NUM = atoi(argv[i + 1]);
  LOGC(1, c, 'k', "Iterations -------------------------------------- : %d\n",
       V_ITER_NUM);

  i = getoptpos("V_INIT_GRAD_DESCENT_STEP_SIZE", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_INIT_GRAD_DESCENT_STEP_SIZE = atof(argv[i + 1]);
  LOGC(1, c, 'k', "Initial Grad Descent Step Size ------------------ : %lf\n",
       (double)V_INIT_GRAD_DESCENT_STEP_SIZE);

  i = getoptpos("V_FINAL_INV_TEMP", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_FINAL_INV_TEMP = atof(argv[i + 1]);
  LOGC(1, c, 'k', "Inverse of Final Temperature -------------------- : %lf\n",
       (double)V_FINAL_INV_TEMP);

  i = getoptpos("V_REST_SHRINK_RATE", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_REST_SHRINK_RATE = atof(argv[i + 1]);
  LOGC(1, c, 'k', "Shrink Rate for Customer Number in Restaurant --- : %lf\n",
       (double)V_REST_SHRINK_RATE);

  i = getoptpos("V_REST_INTERVAL_SIZE", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_REST_INTERVAL_SIZE = atof(argv[i + 1]);
  LOGC(1, c, 'k', "Interval Size between Shrink in Restaurant ------ : %lf\n",
       (double)V_REST_INTERVAL_SIZE);

  i = getoptpos("V_REST_MAX_TABLE_SIZE", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_REST_MAX_TABLE_SIZE = atof(argv[i + 1]);
  LOGC(1, c, 'k', "Max Number of Tables in Restaurant -------------- : %lf\n",
       (double)V_REST_MAX_TABLE_SIZE);

  i = getoptpos("V_L2_REGULARIZATION_WEIGHT", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_L2_REGULARIZATION_WEIGHT = atof(argv[i + 1]);
  LOGC(1, c, 'k', "L2 Regularization Weight ------------------------ : %lf\n",
       (double)V_L2_REGULARIZATION_WEIGHT);

  i = getoptpos("V_MODEL_PROJ_BALL_NORM", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_MODEL_PROJ_BALL_NORM = atof(argv[i + 1]);
  LOGC(1, c, 'k', "Project Model Inside Ball with Norm ------------- : %lf\n",
       (double)V_MODEL_PROJ_BALL_NORM);

  i = getoptpos("V_VOCAB_HIGH_FREQ_CUTOFF", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_VOCAB_HIGH_FREQ_CUTOFF = atoi(argv[i + 1]);
  LOGC(1, c, 'k', "Vocabulary high-freq words cut-off -------------- : %d\n",
       V_VOCAB_HIGH_FREQ_CUTOFF);

  i = getoptpos("V_TEXT_LOWER", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_TEXT_LOWER = atoi(argv[i + 1]);
  LOGC(1, c, 'k', "Convert uppercase to lowercase letters ---------- : %d\n",
       V_TEXT_LOWER);

  i = getoptpos("V_TEXT_RM_TRAIL_PUNC", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_TEXT_RM_TRAIL_PUNC = atoi(argv[i + 1]);
  LOGC(1, c, 'k', "Remove trailing punctuation --------------------- : %d\n",
       V_TEXT_RM_TRAIL_PUNC);

  i = getoptpos("V_CACHE_INTERMEDIATE_MODEL", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_CACHE_INTERMEDIATE_MODEL = atoi(argv[i + 1]);
  LOGC(1, c, 'k', "Cache intermediate models ----------------------- : %d\n",
       V_CACHE_INTERMEDIATE_MODEL);

  i = getoptpos("V_VOCAB_OVERWRITE", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_VOCAB_OVERWRITE = atoi(argv[i + 1]);
  LOGC(1, c, 'k', "Overwrite vocabulary file ----------------------- : %d\n",
       V_VOCAB_OVERWRITE);

  i = getoptpos("V_MODEL_LOAD", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V_MODEL_LOAD = atoi(argv[i + 1]);
  LOGC(1, c, 'k', "Overwrite model file ---------------------------- : %d\n",
       V_MODEL_LOAD);

  i = getoptpos("N", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) N = atoi(argv[i + 1]);
  LOGC(1, c, 'k', "N -- word embedding dim ------------------------- : %d\n",
       N);

  i = getoptpos("V", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) V = atoi(argv[i + 1]);
  LOGC(1, c, 'k', "V -- vocabulary size cap ------------------------ : %d\n",
       V);

  i = getoptpos("C", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) C = atoi(argv[i + 1]);
  LOGC(1, c, 'k', "C -- context length ----------------------------- : %d\n",
       C);

  i = getoptpos("P", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) P = atoi(argv[i + 1]);
  LOGC(1, c, 'k', "P -- context length ----------------------------- : %d\n",
       P);

  i = getoptpos("alpha", argc, argv);
  c = (i == -1) ? 'w' : 'r';
  if (i != -1) alpha = atof(argv[i + 1]);
  LOGC(1, c, 'k', "alpha -- CRP prior ------------------------------ : %d\n",
       (double)alpha);

  LOGC(1, 'g', 'k', "== Sanity Checks ==\n");
  int x = 0;
  x = (NUP > N);
  LOG(1, "        NUP > N: %s (%d > %d)\n", x == 1 ? "yes" : "no", NUP, N);
  if (x == 0) {
    LOG(0, "fail!");
    exit(1);
  }
  x = (VUP > V);
  LOG(1, "        VUP > V: %s (%d > %d)\n", x == 1 ? "yes" : "no", VUP, V);
  if (x == 0) {
    LOG(0, "fail!");
    exit(1);
  }
  x = (PUP > P);
  LOG(1, "        PUP > V: %s (%d > %d)\n", x == 1 ? "yes" : "no", PUP, V);
  if (x == 0) {
    LOG(0, "fail!");
    exit(1);
  }

  LOGC(0, 'g', 'k', "Variable Initialization\n");
  // use perm file instead of original
  /* V_TEXT_FILE_PATH = sformat("%s.perm", V_TEXT_FILE_PATH); */

  // build vocab if necessary, load, and set V by smaller actual size
  if (!fexists(V_VOCAB_FILE_PATH) || V_VOCAB_OVERWRITE) {
    vcb = TextBuildVocab(V_TEXT_FILE_PATH, V_TEXT_LOWER, V_TEXT_RM_TRAIL_PUNC,
                         -1);
    TextSaveVocab(V_VOCAB_FILE_PATH, vcb);
    VocabFree(vcb);
  }
  vcb = TextLoadVocab(V_VOCAB_FILE_PATH, V, V_VOCAB_HIGH_FREQ_CUTOFF);  // >>
  if (V != vcb->size) {
    LOGC(0, 'r', 'k', "[VOCAB]: overwrite V from %d to %d\n", V, vcb->size);
    V = vcb->size;
  }
  // load model if necessary,  otherwise init model with random values
  if (V_MODEL_LOAD && fexists(V_MODEL_SAVE_PATH)) {
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
    rests = RestCreate((long)V * 30, N, -model_init_amp, model_init_amp,
                       V_REST_SHRINK_RATE, V_L2_REGULARIZATION_WEIGHT,
                       (long)V_REST_MAX_TABLE_SIZE,
                       (long)V_REST_INTERVAL_SIZE);  // >>
    w_embd = NumNewHugeVec(V * N);                   // >>
    NumRandFillVec(w_embd, -model_init_amp, model_init_amp, V * N);
  }
  // initialization for negative sampler
  ns = NsInitFromVocabulary(vcb, 0.75, 1, 5);  // >>

  gd_ss = V_INIT_GRAD_DESCENT_STEP_SIZE;  // gd_ss
  inv_temp = 1;
  progress = (real *)malloc(V_THREAD_NUM * sizeof(real));  // >>
  NumFillZeroVec(progress, V_THREAD_NUM);
  start_clock_t = clock();
  LOGC(0, 'g', 'k', "Variable Initialization Finished\n");
  return;
}

void VariableFree() {
  free(progress);   // <<
  NsFree(ns);       // <<
  free(w_embd);     // <<
  RestFree(rests);  // <<
  VocabFree(vcb);   // <<
}

#endif /* ifndef VARIABLES */
