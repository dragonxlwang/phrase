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
#include "constants.c"

#define CHECK_ALLOCATE_STATUS(x) \
  if (!x) LOG(0, "Error: Allocation failed for " #x "\n");

pthread_mutex_t rest_lock, free_lock;

typedef struct Restaurant {
  char **id2table;  // do not need to rewrite
  real *id2cnum;
  real **id2embd;
  int *id2next;  // hash dependent
  int *hash2head;
  volatile long size;  // can be changed by other threads
  long cap;
  long customer_cnt;
  //////////////////////////////
  int embd_dim;
  real *default_embd;
  real shrink_rate;
  real l2w;
  long max_table_num;
  long interval_size;
  //////////////////////////////
} Restaurant;

//// Allocation

char **RestAllocId2Table(long cap) {
  int i;
  const int MAX_PHRASE_LENGTH = WUP * PUP * 3;
  char **id2table = (char **)malloc(cap * sizeof(char *));
  for (i = 0; i < cap; i++) {
    id2table[i] = (char *)malloc(MAX_PHRASE_LENGTH * sizeof(char));
    memset(id2table[i], 0, MAX_PHRASE_LENGTH * sizeof(char));
  }
  return id2table;
}

real *RestAllocId2Cnum(long cap) {
  real *id2cnum = NumNewVec(cap);
  NumFillZeroVec(id2cnum, cap);
  return id2cnum;
}

real **RestAllocId2Embd(long cap, real *default_embd, int embd_dim) {
  int i;
  real **id2embd = (real **)malloc(cap * sizeof(real *));
  for (i = 0; i < cap; i++) {
    id2embd[i] = NumCloneVec(default_embd, embd_dim);
  }
  return id2embd;
}

int *RestAllocId2Next(long cap) {
  int *id2next = NumNewIntVec(cap);
  NumFillZeroIntVec(id2next, cap);
  return id2next;
}

int *RestAllocHash2Head(long cap) {
  int *hash2head = NumNewIntVec(cap);
  memset(hash2head, 0xFF, cap * sizeof(int));
  return hash2head;
}

//// Resize

char **RestReallocId2Table(char **id2table, long old_size, long new_cap,
                           pair *p) {
  char **new_id2table = RestAllocId2Table(new_cap);
  int i;
  if (p) {
    for (i = 0; i < old_size; i++) strcpy(new_id2table[i], id2table[p[i].key]);
  } else {
    for (i = 0; i < old_size; i++) strcpy(new_id2table[i], id2table[i]);
  }
  return new_id2table;
}

real *RestReallocId2Cnum(real *id2cnum, long old_size, long new_cap, pair *p) {
  real *new_id2cnum = RestAllocId2Cnum(new_cap);
  if (p) {
    int i;
    for (i = 0; i < old_size; i++) new_id2cnum[i] = id2cnum[p[i].key];
  } else {
    NumCopyVec(new_id2cnum, id2cnum, old_size);
  }
  return new_id2cnum;
}

real **RestReallocId2Embd(real **id2embd, long old_size, long new_cap,
                          real *default_embd, int embd_dim, pair *p) {
  real **new_id2embd = RestAllocId2Embd(new_cap, default_embd, embd_dim);
  int i;
  if (p) {
    for (i = 0; i < old_size; i++)
      NumCopyVec(new_id2embd[i], id2embd[p[i].key], embd_dim);
  } else {
    for (i = 0; i < old_size; i++)
      NumCopyVec(new_id2embd[i], id2embd[i], embd_dim);
  }
  return new_id2embd;
}

//// Hash (make following macro because of volatile variables)

#define REST_BKDR_HASH(S, CAP)                                         \
  ({                                                                   \
    unsigned long _h = 0;                                              \
    char _ch;                                                          \
    char *_str = S;                                                    \
    while ((_ch = *(_str++))) _h = (_h << 7) + (_h << 1) + (_h) + _ch; \
    _h %= CAP;                                                         \
    _h;                                                                \
  })
#define REST_BKDR_LINK_WITH_HASH(H, I, ID2NEXT, HASH2HEAD) \
  ({                                                       \
    ID2NEXT[I] = HASH2HEAD[H];                             \
    HASH2HEAD[H] = I;                                      \
  })

#define REST_BKDR_LINK(S, I, CAP, ID2NEXT, HASH2HEAD) \
  ({ REST_BKDR_LINK_WITH_HASH(REST_BKDR_HASH(S, CAP), I, ID2NEXT, HASH2HEAD); })

#define REST_BKDR_FOLLOW_HASH_LINKED_LIST(H, S, ID2TABLE, ID2NEXT, HASH2HEAD, \
                                          SIZE)                               \
  ({                                                                          \
    int _i = HASH2HEAD[H];                                                    \
    while (_i != -1 && _i < SIZE && !strcmp(ID2TABLE[_i], S)) {               \
      _i = ID2NEXT[_i];                                                       \
    }                                                                         \
    (_i < SIZE) ? _i : -1;                                                    \
  })

#define REST_LOCATE(R, S)                                                    \
  ({                                                                         \
    REST_BKDR_FOLLOW_HASH_LINKED_LIST(REST_BKDR_HASH(S, R->cap), S,          \
                                      R->id2table, R->id2next, R->hash2head, \
                                      R->size);                              \
  })

#define REST_LOCATE_AND_ADD(R, S)                              \
  ({                                                           \
    if (R->customer_cnt++ > R->interval_size) RestReduce(&R);  \
    if (R->size >= 0.8 * R->cap) RestResize(&R, 2 * R->cap);   \
    int _i = REST_LOCATE(R, S);                                \
    if (_i == -1) {                                            \
      _i = R->size++;                                          \
      strcpy(R->id2table[_i], S);                              \
      R->id2cnum[_i] = 1;                                      \
      REST_BKDR_LINK(S, _i, R->cap, R->id2next, R->hash2head); \
    } else if (_i < R->size) {                                 \
      R->id2cnum[_i] += 1;                                     \
    }                                                          \
    _i;                                                        \
  })

#define REST_ID2CNUM(R, I) ((I == -1 || I >= R->size) ? 0 : R->id2cnum[I])

// A dangerous case: when copying the embd to dest, the restaurant is either
// reduced or resized and thus the old address might not be valid anymore;
// to address this issue, two measurement:
//  1. add free_lock, actually to just check if it is locked and if not, unlock
//     immediately; or otherwise wait until the lock is freed;
//  2. use Restaurant ** instead of Restaurant *, and thus *R_PTR should always
//     be valid
//  Currently, we ony use the 2nd measurement;
#define REST_ID2EMBD_COPY(DEST, R_PTR, I)                                     \
  NumCopyVec(DEST, ((I == -1 || I >= (*R_PTR)->size) ? (*R_PTR)->default_embd \
                                                     : (*R_PTR)->id2embd[I]), \
             (*R_PTR)->embd_dim)

//// check allocation

#define REST_CHECK_ALLOCATION(R)            \
  ({                                        \
    CHECK_ALLOCATE_STATUS(R->id2table);     \
    CHECK_ALLOCATE_STATUS(R->id2cnum);      \
    CHECK_ALLOCATE_STATUS(R->id2embd);      \
    CHECK_ALLOCATE_STATUS(R->id2next);      \
    CHECK_ALLOCATE_STATUS(R->hash2head);    \
    CHECK_ALLOCATE_STATUS(R->default_embd); \
  })

//// check norm

#define REST_NORM(R)                                        \
  ({                                                        \
    int _i;                                                 \
    real _SUM_NORM = 0;                                     \
    for (_i = 0; _i < R->size; _i++) {                      \
      _SUM_NORM += NumVecNorm(R->id2embd[_i], R->embd_dim); \
    }                                                       \
    _SUM_NORM;                                              \
  })

//// sort

pair *REST_SORTED_PAIRS;
long REST_SORTED_LEN = 0;
pair *RestSort(Restaurant *r, int size) {
  int i;
  // resize only if need larger space
  if (size > REST_SORTED_LEN) {
    REST_SORTED_PAIRS = realloc(REST_SORTED_PAIRS, size * sizeof(pair));
    REST_SORTED_LEN = size;
  }
  for (i = 0; i < size; i++) {
    REST_SORTED_PAIRS[i].key = i;
    REST_SORTED_PAIRS[i].val = r->id2cnum[i];
  }
  sort_tuples(REST_SORTED_PAIRS, size, 1);
  return REST_SORTED_PAIRS;
}

Restaurant *RestCreate(long cap, int embd_dim, real init_lb, real init_ub,
                       real shrink_rate, real l2w, long max_table_num,
                       long interval_size) {
  LOG(2, "[Restaurant] Create with cap = %ld\n", cap);

  int i;
  Restaurant *r = (Restaurant *)malloc(sizeof(Restaurant));
  if (cap < 0) cap = 0xFFFFF;  // default 1M

  r->size = 0;
  r->cap = cap;
  r->customer_cnt = 0;
  r->embd_dim = embd_dim;
  r->default_embd = NumNewVec(embd_dim);
  NumRandFillVec(r->default_embd, embd_dim, init_lb, init_ub);
  r->shrink_rate = shrink_rate;
  r->l2w = l2w;
  r->max_table_num = max_table_num;
  r->interval_size = interval_size;

  r->id2table = RestAllocId2Table(cap);
  r->id2cnum = RestAllocId2Cnum(cap);
  r->id2embd = RestAllocId2Embd(cap, r->default_embd, r->embd_dim);
  r->id2next = RestAllocId2Next(cap);
  r->hash2head = RestAllocHash2Head(cap);

  REST_CHECK_ALLOCATION(r);

  if (pthread_mutex_init(&rest_lock, NULL) != 0) {
    LOG(0, "[Restaurant] Mutex init failed\n");
    return NULL;
  }

  if (pthread_mutex_init(&free_lock, NULL) != 0) {
    LOG(0, "[Restaurant] Mutex init failed\n");
    return NULL;
  }

  LOG(2, "[Restaurant] Create Finished\n");
  return r;
}

void RestFree(Restaurant *r) {
  int i;
  for (i = 0; i < r->cap; i++) free(r->id2table[i]);
  for (i = 0; i < r->cap; i++) free(r->id2embd[i]);
  free(r->id2table);
  free(r->id2cnum);
  free(r->id2embd);
  free(r->id2next);
  free(r->hash2head);
  free(r->default_embd);
  free(r);
  return;
}

void RestResize(Restaurant **rptr, int cap) {
  if (pthread_mutex_trylock(&rest_lock) != 0) {
    return;  // mutex not locked
  }

  LOG(0, "resize lock");

  Restaurant *r = *rptr;
  // only resize to a larger size
  if (cap <= r->cap) {
    exit(1);
  }
  int i;

  Restaurant *nr = (Restaurant *)malloc(sizeof(Restaurant));
  nr->size = r->size;  // snapshot
  nr->cap = cap;
  nr->customer_cnt = 0;
  nr->embd_dim = r->embd_dim;
  nr->default_embd = NumCloneVec(r->default_embd, r->embd_dim);
  nr->shrink_rate = r->shrink_rate;
  nr->l2w = r->l2w;
  nr->max_table_num = r->max_table_num;
  nr->interval_size = r->interval_size;

  nr->id2table = RestReallocId2Table(r->id2table, nr->size, nr->cap, NULL);
  nr->id2cnum = RestReallocId2Cnum(r->id2cnum, nr->size, nr->cap, NULL);
  nr->id2embd = RestReallocId2Embd(r->id2embd, nr->size, nr->cap,
                                   nr->default_embd, nr->embd_dim, NULL);
  nr->id2next = RestAllocId2Next(nr->cap);
  nr->hash2head = RestAllocHash2Head(nr->cap);
  for (i = 0; i < nr->size; i++) {
    REST_BKDR_LINK(nr->id2table[i], i, nr->cap, nr->id2next, nr->hash2head);
  }

  REST_CHECK_ALLOCATION(nr);

  // add free_lock to avoid accessing invalid restaurant
  /* pthread_mutex_lock(&free_lock); */
  *rptr = nr;
  RestFree(r);
  /* pthread_mutex_unlock(&free_lock); */

  pthread_mutex_unlock(&rest_lock);

  LOG(0, "resize unlock");

  return;
}

int reduce_cnt = 0;
void RestReduce(Restaurant **rptr) {
  if (pthread_mutex_trylock(&rest_lock) != 0) {
    return;  // mutex not locked
  }

  LOG(0, "reduce lock\n");

  Restaurant *r = *rptr;
  // reduce the number of table to max_table_num
  int i, j, k;

  LOGDBG("Reduce cust=%d size=%d interval=%d pid=%ld\n", (int)r->customer_cnt,
         (int)r->size, (int)r->interval_size, (long int)pthread_self());

  if (r->size <= r->max_table_num) {
    r->customer_cnt = 0;
    for (i = 0; i < r->size; i++) {
      r->id2cnum[i] *= r->shrink_rate;
      if (r->l2w > 0) NumVecMulC(r->id2embd[i], 1 - r->l2w, r->embd_dim);
    }
  } else {
    pair *sorted_pairs = RestSort(r, r->size);  // no need to free

    LOGDBG("[%d] sorting: size = %d\n", reduce_cnt, r->size);

    Restaurant *nr = (Restaurant *)malloc(sizeof(Restaurant));
    nr->size = r->max_table_num / 2;
    nr->cap = r->cap;
    nr->customer_cnt = 0;
    nr->embd_dim = r->embd_dim;
    nr->default_embd = NumCloneVec(r->default_embd, r->embd_dim);
    nr->shrink_rate = r->shrink_rate;
    nr->l2w = r->l2w;
    nr->max_table_num = r->max_table_num;
    nr->interval_size = r->interval_size;

    nr->id2table =
        RestReallocId2Table(r->id2table, nr->size, nr->cap, sorted_pairs);
    nr->id2cnum =
        RestReallocId2Cnum(r->id2cnum, nr->size, nr->cap, sorted_pairs);
    nr->id2embd =
        RestReallocId2Embd(r->id2embd, nr->size, nr->cap, nr->default_embd,
                           nr->embd_dim, sorted_pairs);
    nr->id2next = RestAllocId2Next(nr->cap);
    nr->hash2head = RestAllocHash2Head(nr->cap);
    for (i = 0; i < nr->size; i++) {
      REST_BKDR_LINK(nr->id2table[i], i, nr->cap, nr->id2next, nr->hash2head);
    }

    REST_CHECK_ALLOCATION(nr);

    // add free_lock to avoid accessing invalid restaurant
    /* pthread_mutex_lock(&free_lock); */
    *rptr = nr;
    RestFree(r);
    /* pthread_mutex_unlock(&free_lock); */

    reduce_cnt++;
  }

  pthread_mutex_unlock(&rest_lock);

  LOG(0, "reduce unlock\n");

  return;
}

// use real to return since we need it for compute probability anyways

void RestSave(Restaurant *r, real *w_embd, int v, int n, int iter_num,
              char *fp) {
  char *rfp;
  if (iter_num == -1) {  // save final model, call by master
    rfp = sclone(fp);    // so that free(mfp) can work
  } else {               // avoid thread racing
    char *rdp = sformat("%s.dir", fp);
    if (!direxists(rdp)) dirmake(rdp);
    rfp = sformat("%s/%d.rest", rdp, iter_num);
    free(rdp);
    if (fexists(rfp)) return;
    // do not cache when there is a large chance the restaurant will be modified
    // when saving
    if (r->size >= 0.9 * r->cap - 1e3) return;
    if (r->customer_cnt >= r->interval_size / 3) return;
  }

  pthread_mutex_lock(&rest_lock);

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
  fwrite(&r->l2w, sizeof(real), 1, fout);
  fwrite(&r->max_table_num, sizeof(long), 1, fout);
  fwrite(&r->interval_size, sizeof(long), 1, fout);
  //////////////////////////////
  fwrite(&size, sizeof(long), 1, fout);
  fwrite(&cap, sizeof(long), 1, fout);
  fwrite(&r->customer_cnt, sizeof(long), 1, fout);
  for (i = 0; i < size; i++) fprintf(fout, "%s\n", r->id2table[i]);
  fwrite(r->id2cnum, sizeof(int), size, fout);
  for (i = 0; i < size; i++)
    fwrite(r->id2embd[i], sizeof(real), r->embd_dim, fout);
  fwrite(r->id2next, sizeof(int), size, fout);
  fwrite(r->hash2head, sizeof(int), cap, fout);

  fwrite(&v, sizeof(int), 1, fout);
  fwrite(&n, sizeof(int), 1, fout);
  fwrite(w_embd, sizeof(real), v * n, fout);
  fclose(fout);
  free(rfp);
  if (iter_num == -1) {  // print only when saving final model
    LOGC(1, 'c', 'k', "[REST]: Save to %s\n", fp);
    LOGC(1, 'c', 'k', "[REST]: size = %d, cap = %d, v = %d, n = %d\n", size,
         cap, v, n);
  }

  pthread_mutex_unlock(&rest_lock);

  return;
}

Restaurant *RestLoad(char *fp, real **w_embd_ptr, int *nptr, int *vptr) {
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
  sfread(&r->l2w, sizeof(real), 1, fin);
  sfread(&r->max_table_num, sizeof(long), 1, fin);
  sfread(&r->interval_size, sizeof(long), 1, fin);
  //////////////////////////////
  sfread((void *)&r->size, sizeof(long), 1, fin);
  sfread(&r->cap, sizeof(long), 1, fin);
  sfread(&r->customer_cnt, sizeof(long), 1, fin);
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
  int v, n;
  sfread(&v, sizeof(int), 1, fin);
  sfread(&n, sizeof(int), 1, fin);
  *w_embd_ptr = NumNewHugeVec(v * n);
  sfread(*w_embd_ptr, sizeof(int), v * n, fin);
  *vptr = v;
  *nptr = n;
  return r;
}

#endif /* ifndef TCRP */
