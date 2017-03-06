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
  char **id2table;  // do not need to rewrite
  real *id2cnum;
  real **id2embd;
  int *id2next;  // hash dependent
  int *hash2head;
  int size;  // scalar
  int cap;
  int customer_cnt;
  //////////////////////////////
  int embd_dim;
  real *default_embd;
  real shrink_rate;
  real max_table_num;
  real interval_size;
  //////////////////////////////
  int reduce_lock;
} Restaurant;

Restaurant *RestCreate(int cap, int embd_dim, real init_lb, real init_ub,
                       real shrink_rate, real max_table_num,
                       int interval_size) {
  int i;
  Restaurant *r = (Restaurant *)malloc(sizeof(Restaurant));
  if (cap < 0) cap = 0xFFFFF;  // default 1M
  r->id2table = (char **)malloc(cap * sizeof(char *));
  r->id2cnum = NumNewVec(cap);
  r->id2embd = (real **)malloc(cap * sizeof(real *));
  r->id2next = NumNewIntVec(cap);
  r->hash2head = NumNewIntVec(cap);
  memset(r->hash2head, 0xFF, cap * sizeof(int));
  r->size = 0;
  r->cap = cap;
  r->customer_cnt = 0;
  r->embd_dim = embd_dim;
  r->default_embd = NumNewVec(embd_dim);
  NumRandFillVec(r->default_embd, embd_dim, init_lb, init_ub);
  for (i = 0; i < cap; i++)
    r->id2embd[i] = NumCloneVec(r->default_embd, r->embd_dim);
  if (!r->id2table || !r->id2cnum || !r->id2embd || !r->id2next ||
      !r->hash2head || r->default_embd) {
    LOG(0, "[Restaurant]: allocation error\n");
    exit(1);
  }
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
  // Only one thread will resize the restaurant
  int i, old_cap;
  int *id2next, *hash2head;
  // safeguard for thread racing
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

pair *RestSort(Restaurant *r, int sort_size) {
  int i;
  real *arr = (real *)malloc(sort_size * sizeof(real));
  for (i = 0; i < sort_size; i++) arr[i] = r->id2cnum[i];
  pair *sorted_pairs = sorted(arr, sort_size, 1);
  free(arr);
  return sorted_pairs;
}

void RestReduce(Restaurant *r) {
  // reduce the number of table to max_table_num
  int i, j;
  // safeguard for thread racing
  if (r->customer_cnt < r->interval_size) return;
  r->customer_cnt = 0;
  if (r->size <= r->max_table_num) {  // only shrink, no table remove
    for (i = 0; i < r->size; i++) r->id2cnum[i] *= r->shrink_rate;
  } else {  // shrink and remove extra tables
    int cap = r->cap;
    int sort_size = r->size;
    pair *sorted_pairs = RestSort(r, sort_size);
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
    for (i = r->max_table_num; i < sort_size; i++)
      id2embd[i] = r->id2embd[sorted_pairs[i].key];
    for (i = sort_size; i < cap; i++) id2embd[i] = r->id2embd[i];
    // update rest
    char **old_id2table = r->id2table;
    real *old_id2cnum = r->id2cnum;
    real **old_id2embd = r->id2embd;
    int *old_id2next = r->id2next;
    int *old_hash2head = r->hash2head;
    int cur_size = r->size;
    r->size = r->max_table_num;
    r->id2table = id2table;
    r->id2cnum = id2cnum;
    r->id2embd = id2embd;
    r->id2next = id2next;
    r->hash2head = hash2head;
    // delete tables with id larger than max_table_num
    for (i = r->max_table_num; i < cur_size; i++) free(old_id2table[i]);
    free(old_id2table);
    free(old_id2cnum);
    free(old_id2embd);
    free(old_id2next);
    free(old_hash2head);
    free(sorted_pairs);
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
  if (r->customer_cnt++ > r->interval_size) RestReduce(r);
  while (i != -1 && !strcmp(r->id2table[i], pstr)) i = r->id2next[i];
  if (i == -1) {
    if (r->size >= 0.9 * r->cap) {
      RestResize(r, 2 * r->cap);
      h = DictBkdrHash(pstr) % r->cap;
    }
    i = r->size++;
    r->id2table[i] = sclone(pstr);
    r->id2next[i] = r->hash2head[h];
    r->hash2head[h] = i;
    r->id2cnum[i] = 1;
    NumCopyVec(r->id2embd[i], r->default_embd, r->embd_dim);
  } else
    r->id2cnum[i]++;
  return i;
}

real RestId2Cnum(Restaurant *r, int i) { return (i == -1) ? 0 : r->id2cnum[i]; }

real *RestId2Embd(Restaurant *r, int i) {
  return (i == -1) ? r->default_embd : r->id2embd[i];
}

void RestSave(Restaurant *r, int iter_num, char *fp) {
  char *rfp;
  if (iter_num == -1) {  // save final model, call by master
    rfp = sclone(fp);    // so that free(mfp) can work
  } else {               // avoid thread racing
    char *rdp = sformat("%s.dir", fp);
    if (!direxists(rdp)) dirmake(rdp);
    rfp = sformat("%s/%d.iter", rdp, iter_num);
    free(rdp);
    if (fexists(rfp)) return;
    // do not cache when there is a large chance the restaurant will be modified
    // when saving
    if (r->size >= 0.9 * r->cap - 1e3) return;
    if (r->customer_cnt >= r->interval_size / 3) return;
  }
  FILE *fout = fopen(rfp, "wb");
  if (!fout) {
    LOG(0, "Error!\n");
    exit(1);
  }
  int i;
  int size = r->size;
  int cap = r->cap;
  fwrite(&r->embd_dim, sizeof(int), 1, fout);
  fwrite(r->default_embd, sizeof(real), r->embd_dim, fout);
  fwrite(&r->shrink_rate, sizeof(real), 1, fout);
  fwrite(&r->max_table_num, sizeof(real), 1, fout);
  fwrite(&r->interval_size, sizeof(real), 1, fout);
  //////////////////////////////
  fwrite(&size, sizeof(int), 1, fout);
  fwrite(&cap, sizeof(int), 1, fout);
  fwrite(&r->customer_cnt, sizeof(int), 1, fout);
  for (i = 0; i < size; i++) fprintf(fout, "%s\n", r->id2table[i]);
  fwrite(r->id2cnum, sizeof(int), size, fout);
  for (i = 0; i < size; i++)
    fwrite(r->id2embd[i], sizeof(real), r->embd_dim, fout);
  fwrite(r->id2next, sizeof(int), size, fout);
  fwrite(r->hash2head, sizeof(int), cap, fout);
  fclose(fout);
  free(rfp);
  if (iter_num == -1) {  // print only when saving final model
    LOGC(1, 'c', 'k', "[REST]: Save to %s\n", fp);
    LOGC(1, 'c', 'k', "[REST]: size = %d, cap = %d\n", size, cap);
  }
  return;
}

Restaurant *RestLoad(char *fp) {
  FILE *fin = fopen(fp, "rb");
  if (!fin) {
    LOG(0, "Error!\n");
    exit(1);
  }
  char str[0xFFFF];
  int i;
  Restaurant *r = (Restaurant *)malloc(sizeof(Restaurant));
  sfread(&r->embd_dim, sizeof(int), 1, fin);
  r->default_embd = NumNewVec(r->embd_dim);
  sfread(r->default_embd, sizeof(real), r->embd_dim, fin);
  sfread(&r->shrink_rate, sizeof(real), 1, fin);
  sfread(&r->max_table_num, sizeof(real), 1, fin);
  sfread(&r->interval_size, sizeof(real), 1, fin);
  //////////////////////////////
  sfread(&r->size, sizeof(int), 1, fin);
  sfread(&r->cap, sizeof(int), 1, fin);
  sfread(&r->customer_cnt, sizeof(int), 1, fin);
  r->id2table = (char **)malloc(r->cap * sizeof(char *));
  for (i = 0; i < r->size; i++) {
    fscanf(fin, "%s\n", str);
    r->id2table[i] = sclone(str);
  }
  r->id2cnum = NumNewVec(r->cap);
  sfread(r->id2cnum, sizeof(int), r->size, fin);
  r->id2embd = (real **)malloc(r->cap * sizeof(real *));
  for (i = 0; i < r->size; i++) {
    r->id2embd[i] = NumNewVec(r->embd_dim);
    sfread(r->id2embd[i], sizeof(real), r->embd_dim, fin);
  }
  for (i = r->size; i < r->cap; i++)
    r->id2embd[i] = NumCloneVec(r->default_embd, r->embd_dim);
  r->id2next = NumNewIntVec(r->cap);
  sfread(r->id2next, sizeof(int), r->size, fin);
  r->hash2head = NumNewIntVec(r->cap);
  sfread(r->hash2head, sizeof(int), r->cap, fin);
  return r;
}

// not used
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
