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

real RestaurantInquiry(Dictionary *r, char *p) { return DictGet(r, p, 0); }

void RestaurantAdd(Dictionary *r, char *p) {
  DictIncrement(r, p, 1, 0);
  return;
}

void RestaurantReduce(Dictionary *r, int max_table_num, real shrink_rate) {
  pair *sorted_pairs;
  DictReduce(r, max_table_num, &sorted_pairs);
  DictShrink(r, shrink_rate);
  return;
}

#endif /* ifndef TCRP */
