#ifndef TCRP
#define TCRP

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

typedef struct Restaurant {
  char **id2table;
  real *id2cnum;
  real **id2embd;
  int *id2next;
  int *hash2head;
  int size;
  int cap;
  int customer_cnt;
  //////////////////////////////
  int embd_dim;
  real *default_embd;
  real shrink_rate;
  real max_table_num;
  real interval_size;
  int reduce_lock;
} Restaurant;

Restaurant *RestCreate(int cap, int embd_dim, real init_lb, real init_ub,
                       real shrink_rate, real max_table_num,
                       int interval_size) {
  Restaurant *r = (Restaurant *)malloc(sizeof(Restaurant));
  if (cap < 0) cap = 0xFFFFF;  // default 1M
  r->id2table = (char **)malloc(cap * sizeof(char *));
  r->id2cnum = NumNewVec(cap);
  r->id2embd = (real **)malloc(cap * sizeof(real *));
  r->id2next = NumNewIntVec(cap);
  r->hash2head = NumNewIntVec(cap);
  r->size = 0;
  r->cap = cap;
  r->embd_dim = embd_dim;
  r->default_embd = NumNewVec(embd_dim);
  NumRandFillVec(r->default_embd, embd_dim, init_lb, init_ub);
  if (!r->id2table || !r->id2cnum || !r->id2embd || !r->id2next ||
      !r->hash2head) {
    LOG(0, "[Restaurant]: allocation error\n");
    exit(1);
  }
  memset(r->hash2head, 0xFF, cap * sizeof(int));
  r->customer_cnt = 0;
  r->shrink_rate = shrink_rate;
  r->max_table_num = max_table_num;
  r->interval_size = interval_size;
  r->reduce_lock = 0;
  return r;
}

void RestFree(Restaurant *r) {
  free(r->id2table);
  free(r->id2cnum);
  free(r->id2embd);
  free(r->id2next);
  free(r->hash2head);
  free(r->default_embd);
  free(r);
  return;
}

void RestBkdrLink(char *s, int i, int cap, int *id2next, int *hash2head) {
  int h = DictBkdrHash(s) % cap;
  id2next[i] = hash2head[h];
  hash2head[h] = i;
  return;
}

void RestResize(Restaurant *r, int cap) {
  // resize the cap (> size) of the dictionary
  int i, old_cap;
  int *id2next, *hash2head;
  if (r->cap >= cap) return;
  old_cap = r->cap;
  r->cap = cap;
  r->id2table = (char **)realloc(r->id2table, r->cap * sizeof(char *));
  r->id2cnum = (real *)realloc(r->id2cnum, r->cap * sizeof(real));
  r->id2embd = (real **)realloc(r->id2embd, r->cap * sizeof(real *));
  for (i = old_cap; i < r->cap; i++)
    r->id2embd[i] = NumCloneVec(r->default_embd, r->embd_dim);
  id2next = NumNewIntVec(r->cap);
  hash2head = NumNewIntVec(r->cap);
  if (!r->id2table || !r->id2cnum || !r->id2embd || !id2next || !hash2head) {
    LOG(0, "[Restaurant]: resize error\n");
    exit(1);
  }
  memset(hash2head, 0xFF, r->cap * sizeof(int));
  for (i = 0; i < r->size; i++)
    RestBkdrLink(r->id2table[i], i, r->cap, id2next, hash2head);
  free(r->id2next);
  free(r->hash2head);
  r->id2next = id2next;
  r->hash2head = hash2head;
  return;
}

pair *RestSort(Restaurant *r) {
  int i;
  real *arr = (real *)malloc(r->size * sizeof(real));
  for (i = 0; i < r->size; i++) arr[i] = r->id2cnum[i];
  return sorted(arr, r->size, 1);
}

void RestReduce(Restaurant *r) {
  int i, j;
  if (r->max_table_num >= r->size) {  // only shrink, no table remove
    for (i = 0; i < r->size; i++) r->id2cnum[i] *= r->shrink_rate;
  } else {               // shrink and remove extra tables
    int size = r->size;  // cache current size and cap
    int cap = r->cap;
    pair *sorted_pairs = RestSort(r);
    char **id2table = (char **)malloc(cap * sizeof(char *));
    real *id2cnum = NumNewVec(cap);
    real **id2embd = (real **)malloc(cap * sizeof(real *));
    int *id2next = NumNewIntVec(cap);
    int *hash2head = NumNewIntVec(cap);
    memset(hash2head, 0xFF, cap * sizeof(int));
    for (i = 0; i < r->max_table_num; i++) {
      j = sorted_pairs[i].key;
      id2table[i] = r->id2table[j];
      id2cnum[i] = r->id2cnum[j] * r->shrink_rate;
      id2embd[i] = r->id2embd[j];
      RestBkdrLink(id2table[i], i, cap, id2next, hash2head);
    }
    for (i = r->max_table_num; i < size; i++) {
      id2embd[i] = r->id2embd[sorted_pairs[i].key];
      NumCopyVec(id2embd[i], r->default_embd, r->embd_dim);
    }
    for (i = size; i < cap; i++) id2embd[i] = r->id2embd[i];
    char **old_id2table = r->id2table;
    real *old_id2cnum = r->id2cnum;
    real **old_id2embd = r->id2embd;
    int *old_id2next = r->id2next;
    int *old_hash2head = r->hash2head;
    r->id2table = id2table;
    r->id2cnum = id2cnum;
    r->id2embd = id2embd;
    r->id2next = id2next;
    r->hash2head = hash2head;
    for (i = r->max_table_num; i < size; i++) free(old_id2table[i]);
    free(old_id2table);
    free(old_id2cnum);
    free(old_id2embd);
    free(old_id2next);
    free(old_hash2head);
  }
  return;
}

int RestLocate(Restaurant *r, char *pstr) {
  int h = DictBkdrHash(pstr) % r->cap;
  int i = r->hash2head[h];
  while (i != -1 && !strcmp(r->id2table[i], pstr)) i = r->id2next[i];
  return i;
}

int RestLocateAndAdd(Restaurant *r, char *pstr) {
  int h = DictBkdrHash(pstr) % r->cap;
  int i = r->hash2head[h];
  while (i != -1 && !strcmp(r->id2table[i], pstr)) i = r->id2next[i];
  if (i == -1) {
    // double simplified mutex
    if (r->customer_cnt++ > r->interval_size) {
      r->customer_cnt = 0;
      if (!r->reduce_lock) {
        r->reduce_lock = 1;
        RestReduce(r);
        r->reduce_lock = 0;
      }
    }
    if (r->size >= LARGER(0.9 * r->cap, r->cap - 1e3))
      RestResize(r, 2 * r->cap);
    i = r->size++;
    r->id2table[i] = sclone(pstr);
    r->id2next[i] = r->hash2head[h];
    r->hash2head[h] = i;
    r->id2cnum[i] = 1;
    r->id2embd[i] = NumCloneVec(r->default_embd, r->embd_dim);
  } else
    r->id2cnum[i]++;
  return i;
}

real RestId2Cnum(Restaurant *r, int i) { return (i == -1) ? 0 : r->id2cnum[i]; }

real *RestId2Embd(Restaurant *r, int i) {
  return (i == -1) ? r->default_embd : r->id2embd[i];
}

int RestGet(Restaurant *r, char *pstr, real **embd_ptr) {
  int i = RestLocate(r, pstr);
  if (i == -1)
    return 0;
  else {
    *embd_ptr = r->id2embd[i];
    return r->id2cnum[i];
  }
}

int RestGetCnum(Restaurant *r, char *pstr) {
  int i = RestLocate(r, pstr);
  return (i == -1) ? 0 : r->id2cnum[i];
}

real *RestGetEmbd(Restaurant *r, char *pstr) {
  int i = RestLocate(r, pstr);
  return (i == -1) ? NULL : r->id2embd[i];
}

int RestExist(Restaurant *r, char *pstr) { return RestLocate(r, pstr) == -1; }

#endif /* ifndef TCRP */
