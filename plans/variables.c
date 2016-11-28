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
#include "neg_samp.c"
#include "phrase.c"
#include "restuarant.c"

// ---------------------------- Config Variables ------------------------------
char *V_MODEL_DECOR_FILE_PATH = NULL;
char *V_TEXT_FILE_PATH = "~/data/gigaword/giga_nyt.txt";  // "text8/text8"
char *V_VOCAB_FILE_PATH = NULL;  // don't set it if can be inferred from above
char *V_MODEL_SAVE_PATH = NULL;
char *V_PEEK_FILE_PATH = NULL;

int V_THREAD_NUM = 20;
int V_ITER_NUM = 10;

// Initial grad descent step size
real V_INIT_GRAD_DESCENT_STEP_SIZE = 1e-4;
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
int V_CACHE_INTERMEDIATE_MODEL = 0;
// if overwrite vocab file
int V_VOCAB_OVERWRITE = 0;
// if load model from file instead of random initiailization
int V_MODEL_LOAD = 0;
// if overwrite peek file
int V_PEEK_OVERWRITE = 0;
// Peek sampling rate
real V_PEEK_SAMPLE_RATE = 1e-6;
int N = 100;       // embedding dimension
int V = 1000000;   // vocabulary size cap, set to -1 if no limit
int C = 5;         // context length
int P = 4;         // phrase max length
real alpha = 0.1;  // CRP prior

#define PUP 5
#define NUP 100
#define SUP 1000

// ---------------------------- global variables ------------------------------
Vocabulary *vcb;        // vocabulary
real *w_embd;           // word embedding (target only)
Restaurant *rest;       // restaurant
NegativeSampler *ns;    // negative sampler
real gd_ss;             // grad descend step size
real temp;              // temperature
real *progress;         // thread progress
clock_t start_clock_t;  // start time for model training

real GetTrainProgress(real *progress, int thread_num, int iter_num) {
  real p = NumSumVec(progress, thread_num);
  p /= (thread_num * iter_num);
  return p;
}

#endif /* ifndef VARIABLES */
