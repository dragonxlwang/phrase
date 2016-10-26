#ifndef EMBEDDING
#define EMBEDDING

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

typedef struct Embedding {
  real **ll;
  int size;
  int cap;
} Embedding;

void EmbdNewVec(Embedding *e, int dim) {
  // add a zero vector into embedding model
  int i;
  if (e->size == e->cap) {
    e->ll = (real **)realloc(e->ll, 2 * e->cap * sizeof(real *));
    for(i = e->cap; i < 2 * e->cap; i++) e->ll[i] = NumNewVec(dim);
    e->cap *= 2;
  }
  NumFillZeroVec(e->ll[e->size], dim);
  e->size++;
  return;
}

void EmbdReduce(Embedding *e, int new_size, pair *sorted_pairs) {
  int i;
  real **nll = (real **) malloc(e->size * sizeof(real *));
  for(i = 0 ;i < e->size; i++) nll[i] = e->ll[sorted_pairs[i].key];
  e->size = new_size;
}

#endif /* ifndef EMBEDDING */
