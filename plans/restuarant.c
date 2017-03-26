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

void _strcpy(char *dest, char *scr) { strncpy(dest, scr, MAX_PHRASE_LEN); }

pthread_mutex_t rest_lock;
pthread_mutex_t *hash_locks;

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

int _RID = 0;

//// Allocation

char **RestAllocId2Table(long cap) {
  int i;
  char **id2table = (char **)malloc(cap * sizeof(char *));
  for (i = 0; i < cap; i++) {
    id2table[i] = (char *)malloc(MAX_PHRASE_LEN * sizeof(char));
    memset(id2table[i], 0, MAX_PHRASE_LEN * sizeof(char));
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

void RestInitHash2Head(int *hash2head, long cap) {
  memset(hash2head, 0xFF, cap * sizeof(int));
}
int *RestAllocHash2Head(long cap) {
  int *hash2head = NumNewIntVec(cap);
  RestInitHash2Head(hash2head, cap);
  return hash2head;
}

pthread_mutex_t *RestAllocLocks(long cap) {
  pthread_mutex_t *locks =
      (pthread_mutex_t *)malloc(cap * sizeof(pthread_mutex_t));
  int i;
  for (i = 0; i < cap; i++) {
    if (pthread_mutex_init(&(locks[i]), NULL) != 0) {
      LOG(0, "mutex lock at %d init failed\n", i);
      return NULL;
    }
  }
  return locks;
}

//// Resize (always called only in single thread)

void RestReallocId2Table(char **new_id2table, char **id2table, long old_size,
                         pair *p) {
  int i;
  if (p) {
    for (i = 0; i < old_size; i++) _strcpy(new_id2table[i], id2table[p[i].key]);
  } else {
    for (i = 0; i < old_size; i++) _strcpy(new_id2table[i], id2table[i]);
  }
  return;
}

void RestReallocId2Cnum(real *new_id2cnum, real *id2cnum, long old_size,
                        pair *p) {
  int i;
  if (p) {
    for (i = 0; i < old_size; i++) new_id2cnum[i] = id2cnum[p[i].key];
  } else {
    for (i = 0; i < old_size; i++) new_id2cnum[i] = id2cnum[i];
  }
  return;
}

void RestReallocId2Embd(real **new_id2embd, real **id2embd, long old_size,
                        int embd_dim, pair *p) {
  int i;
  if (p) {
    for (i = 0; i < old_size; i++)
      NumCopyVec(new_id2embd[i], id2embd[p[i].key], embd_dim);
  } else {
    for (i = 0; i < old_size; i++)
      NumCopyVec(new_id2embd[i], id2embd[i], embd_dim);
  }
  return;
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

// link string S at position I in restuarnt R
#define REST_BKDR_LINK_NO_LOCK(S, I, R) \
  ({                                    \
    int _h = REST_BKDR_HASH(S, R->cap); \
    R->id2next[I] = R->hash2head[_h];   \
    R->hash2head[_h] = R->id2next[I];   \
  })
#define REST_BKDR_LINK(S, I, R)              \
  ({                                         \
    int _h = REST_BKDR_HASH(S, R->cap);      \
    pthread_mutex_lock(&(hash_locks[_h]));   \
    R->id2next[I] = R->hash2head[_h];        \
    R->hash2head[_h] = R->id2next[I];        \
    pthread_mutex_unlock(&(hash_locks[_h])); \
  })

// TODO(xlwang): might want to append instead of prepend
real _link_total = 1;
real _link_error = 0;
#define REST_LOCATE(R, S)                                             \
  ({                                                                  \
    int _h = REST_BKDR_HASH(S, R->cap);                               \
    int _i = R->hash2head[_h];                                        \
    int _step = 0;                                                    \
    while (_i != -1 && _i < R->size && !strcmp(R->id2table[_i], S) && \
           _step++ < 100) {                                           \
      _i = R->id2next[_i];                                            \
    }                                                                 \
    if (_step >= 100) {                                               \
      _link_error += 1;                                               \
    }                                                                 \
    _link_total += 1;                                                 \
    (_i < R->size && _step < 100) ? _i : -1;                          \
  })

#define REST_LOCATE_AND_ADD(R, S)                               \
  ({                                                            \
    if (R->customer_cnt++ > R->interval_size) RestReduce(R);    \
    if (R->size >= R->cap) {                                    \
      LOG(0, "restaurant size exceeds\n");                      \
      exit(1);                                                  \
    }                                                           \
    int _i = REST_LOCATE(R, S);                                 \
    if (_i == -1) {                                             \
      _i = R->size++;                                           \
      _strcpy(R->id2table[_i], S);                              \
      R->id2cnum[_i] = 1;                                       \
      NumCopyVec(R->id2embd[_i], R->default_embd, R->embd_dim); \
      REST_BKDR_LINK(S, _i, R);                                 \
    } else if (_i < R->size) {                                  \
      R->id2cnum[_i] += 1;                                      \
    }                                                           \
    _i;                                                         \
  })

#define REST_ID2CNUM(R, I) ((I == -1 || I >= R->size) ? 0 : R->id2cnum[I])
#define REST_ID2EMBD(R, I) \
  ((I == -1 || I >= R->size) ? R->default_embd : R->id2embd[I])

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

//// sort (always called in only one thread)

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

//// create (always called in only one thread)

Restaurant *RestCreate(long cap, int embd_dim, real init_lb, real init_ub,
                       real shrink_rate, real l2w, long max_table_num,
                       long interval_size) {
  LOG(2, "[Restaurant] Create with cap = %ld\n", cap);

  int t;
  Restaurant *_rests = (Restaurant *)malloc(sizeof(Restaurant) * 2);
  if (cap < 0) cap = 0xFFFFF;  // default 1M

  Restaurant *r;
  real *default_embd = NumNewVec(embd_dim);
  NumRandFillVec(default_embd, embd_dim, init_lb, init_ub);
  for (t = 0; t < 2; t++) {
    r = &(_rests[t]);
    r->size = 0;
    r->cap = cap;
    r->customer_cnt = 0;
    r->embd_dim = embd_dim;
    r->default_embd = NumCloneVec(default_embd, embd_dim);
    r->shrink_rate = shrink_rate;
    r->l2w = l2w;
    r->max_table_num = max_table_num;
    r->interval_size = interval_size;
    r->id2table = RestAllocId2Table(cap);  // size 2048
    r->id2cnum = RestAllocId2Cnum(cap);
    r->id2embd = RestAllocId2Embd(cap, r->default_embd, r->embd_dim);
    r->id2next = RestAllocId2Next(cap);
    r->hash2head = RestAllocHash2Head(cap);
    hash_locks = RestAllocLocks(cap);  // size 40
    REST_CHECK_ALLOCATION(r);
  }
  _RID = 0;

  if (pthread_mutex_init(&rest_lock, NULL) != 0) {
    LOG(0, "[Restaurant] Mutex init failed\n");
    return NULL;
  }

  free(default_embd);

  LOG(2, "[Restaurant] Create Finished\n");
  LOGC(0, 'b', 'k', "address: %ld %ld, size = %d %d\n", &(_rests[0]),
       &(_rests[1]), _rests[0].size, _rests[1].size);
  return _rests;
}

int reduce_cnt = 0;
void RestReduce(Restaurant *some_rest) {
  if (pthread_mutex_trylock(&rest_lock) != 0) {
    return;  // mutex not locked
  }

  LOG(0, "=======>>> REDUCE LOCK, _RID = %d\n", _RID);
  Restaurant *_rests = ((_RID == 0) ? some_rest : (some_rest - 1));
  Restaurant *r = &(_rests[_RID]);
  Restaurant *nr = &(_rests[1 - _RID]);
  LOGC(0, 'b', 'k', "_RID = %d, size: %d, %d\n", _RID, r->size, nr->size);

  // reduce the number of table to max_table_num
  int i, j, k;

  LOGDBG("Reduce cust=%d size=%d interval=%d pid=%ld _RID=%d ",
         (int)r->customer_cnt, (int)r->size, (int)r->interval_size,
         (long int)pthread_self(), _RID);

  if (r->size <= r->max_table_num) {
    LOGC(0, 'c', 'k', "Shrink only\n");

    r->customer_cnt = 0;
    for (i = 0; i < r->size; i++) {
      r->id2cnum[i] *= r->shrink_rate;
      if (r->l2w > 0) NumVecMulC(r->id2embd[i], 1 - r->l2w, r->embd_dim);
    }
  } else {
    pair *sorted_pairs = RestSort(r, r->size);  // no need to free
    nr->size = r->max_table_num / 2;
    nr->customer_cnt = 0;

    LOGC(0, 'g', 'k', "Remove and Shrink\n");
    LOGC(0, 'y', 'k', "   [%d] size  %d => %d\n", reduce_cnt, r->size,
         nr->size);

    LOGC(0, 'y', 'k', "   Check 1: Pass declaration\n");

    RestReallocId2Table(nr->id2table, r->id2table, nr->size, sorted_pairs);

    LOGC(0, 'y', 'k', "   Check 2.1: id2table finish\n");

    RestReallocId2Cnum(nr->id2cnum, r->id2cnum, nr->size, sorted_pairs);

    LOGC(0, 'y', 'k', "   Check 2.2: id2cnum finish\n");

    RestReallocId2Embd(nr->id2embd, r->id2embd, nr->size, nr->embd_dim,
                       sorted_pairs);

    LOGC(0, 'y', 'k', "   Check 2.3: id2embd finish\n");

    RestInitHash2Head(nr->hash2head, nr->cap);

    LOGC(0, 'y', 'k', "   Check 2.4: has2head finish\n");

    // TODO(xlwang): assuming that at reducing time, the back-up restaurant is
    // not accessed by any other threads
    for (i = 0; i < nr->size; i++) {
      REST_BKDR_LINK_NO_LOCK(nr->id2table[i], i, nr);
    }

    LOGC(0, 'y', 'k', "   Check 3: Pass hash linking\n");

    _RID = 1 - _RID;

    reduce_cnt++;
  }

  LOGC(0, 'y', 'k', "   Linking Errors: %e / %e = %e\n", _link_error,
       _link_total, _link_error / _link_total);
  LOG(0, "=======<<< REDUCE UNLOCK\n");

  pthread_mutex_unlock(&rest_lock);

  return;
}

// use real to return since we need it for compute probability anyways

void RestSave(Restaurant *_rests, real *w_embd, int v, int n, int iter_num,
              char *fp) {
  Restaurant *r = &(_rests[_RID]);
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
  int t, i, v, n;
  char str[MAX_PHRASE_LEN];
  Restaurant *_rests = (Restaurant *)malloc(sizeof(Restaurant) * 2);
  Restaurant *r;
  for (t = 0; t < 2; t++) {
    FILE *fin = fopen(fp, "rb");
    if (!fin) {
      LOG(0, "Error!\n");
      exit(1);
    }
    r = &(_rests[t]);
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
    if (t == 0) {
      sfread(&v, sizeof(int), 1, fin);
      sfread(&n, sizeof(int), 1, fin);
      *w_embd_ptr = NumNewHugeVec(v * n);
      sfread(*w_embd_ptr, sizeof(int), v * n, fin);
      *vptr = v;
      *nptr = n;
      hash_locks = RestAllocLocks(r->cap);
    }
    fclose(fin);
  }
  _RID = 0;
  return _rests;
}

void RestFree(Restaurant *_rests) {
  int t, i;
  Restaurant *r;
  for (t = 0; t < 2; t++) {
    r = &(_rests[t]);
    for (i = 0; i < r->cap; i++) free(r->id2table[i]);
    for (i = 0; i < r->cap; i++) free(r->id2embd[i]);
    free(r->id2table);
    free(r->id2cnum);
    free(r->id2embd);
    free(r->id2next);
    free(r->hash2head);
    free(r->default_embd);
  }
  free(_rests);
  free(hash_locks);
}

#endif /* ifndef TCRP */
