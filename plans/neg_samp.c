#ifndef NEG_SAMP
#define NEG_SAMP

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

typedef struct NegativeSampler {
  int *wrh_a;      // WRH sample alias array
  real *wrh_p;     // WRH sample probability array
  int *table;      // Table sample
  real table_r;    // Table size ratio
  real *prob;      //
  real *prob_log;  //
  int use_wrh;     // use WRH method instead of Table
  int num;         // probability dimensions
  int neg_num;     // number of negative samples
} NegativeSampler;

NegativeSampler *NsInit(real *freq, int num, real ns_pow, int use_wrh,
                        int neg_num) {
  LOG(2, "[NS]: Init\n");
  NegativeSampler *ns = (NegativeSampler *)malloc(sizeof(NegativeSampler));
  ns->use_wrh = use_wrh;
  ns->num = num;
  ns->neg_num = neg_num;
  int i;
  real s = 0;
  ns->prob = NumNewHugeVec(num);
  ns->prob_log = NumNewHugeVec(num);
  for (i = 0; i < num; i++) {
    ns->prob[i] = pow(freq[i], ns_pow);
    s += ns->prob[i];
  }
  for (i = 0; i < num; i++) ns->prob[i] /= s;
  if (use_wrh)
    NumMultinomialWRBInit(ns->prob, num, 1, &ns->wrh_a, &ns->wrh_p);
  else {
    ns->table_r = SMALLER(((real)1e8) / num, 100);
    ns->table = NumMultinomialTableInit(ns->prob, num, ns->table_r);
  }
  for (i = 0; i < num; i++) ns->prob_log[i] = log(neg_num * ns->prob[i]);
  return ns;
}

NegativeSampler *NsInitFromVocabulary(Vocabulary *vcb, real ns_pow, int use_wrh,
                                      int neg_num) {
  real *freq = NumNewVec(vcb->size);
  int i;
  for (i = 0; i < vcb->size; i++) freq[i] = VocabGetCount(vcb, i);
  NegativeSampler *ns = NsInit(freq, vcb->size, ns_pow, use_wrh, neg_num);
  free(freq);
  return ns;
}

int NsSample(NegativeSampler *ns, unsigned long *rs) {
  if (ns->use_wrh)
    return NumMultinomialWRBSample(ns->wrh_a, ns->wrh_p, ns->num, rs);
  else
    return NumMultinomialTableSample(ns->table, ns->num, ns->table_r, rs);
}

void NsFree(NegativeSampler *ns) {
  if (ns->use_wrh) {
    free(ns->wrh_a);
    free(ns->wrh_p);
  } else
    free(ns->table);
  free(ns->table);
  free(ns->prob_log);
  free(ns);
  return;
}

#endif /* ifndef NEG_SAMP */
