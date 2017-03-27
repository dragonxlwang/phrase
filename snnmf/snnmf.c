#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include "../utils/util_misc.c"
#include "../utils/util_num.c"
#include "../utils/util_text.c"
#include "variables.c"
#include "constants.c"

real ** C = NULL; //C:Assitant Matrix to compute B in FISTA

void snnmfInit(){
    B = (real **)malloc(vcb->size * sizeof(real *));
    C = (real **)malloc(vcb->size * sizeof(real *));
	int i =0, j=0;
    for (i=0; i<vcb->size; i++){
        B[i] = (real*) malloc(V_PHRASE_SIZE * sizeof(real));
        C[i] = (real*) malloc(V_PHRASE_SIZE * sizeof(real));
        for (j=0; j<V_PHRASE_SIZE; j++){
            B[i][j] = NumRand();
            C[i][j] = B[i][j];
        }
    }
}

int sid_w2v_ppb_lock = 0;
void W2vThreadPrintProgBar(int dbg_lvl, int tid, real p) {
	if (sid_w2v_ppb_lock) return;
	sid_w2v_ppb_lock = 1;
    LOGCLR(dbg_lvl);
    char *str = malloc(0x1000);
    int i, bar_len = 10;
    clock_t cur_clock_t = clock();
    real pct = p * 100;
    int bar = p * bar_len;
    double st = (double)(cur_clock_t - start_clock_t) / CLOCKS_PER_SEC;
    char *ht = strtime(st / V_THREAD_NUM);
    sprintfc(str, 'y', 'k', "[%7.4lf%%]: ", pct);             // percentage
    for (i = 0; i < bar; i++) saprintfc(str, 'r', 'k', "+");  // bar: past
    saprintfc(str, 'w', 'k', "~");                            // bar: current
    for (i = bar + 1; i < bar_len; i++)                       //
        saprintfc(str, 'c', 'k', "=");                          // bar: left
    saprintf(str, " ");                                       //
    saprintfc(str, 'g', 'k', "(tid=%02d)", tid);              // tid
    saprintf(str, " ");                                       //
    saprintf(str, "TIME:%.2e/%s ", st, ht);                   // time
    saprintf(str, "GDSS:%.4e ", gd_ss);                       // gdss
	if (IS_PRINT_LOSS){	
		real sum = NumSumVec(loss, V_THREAD_NUM);
		saprintf(str, "LOSS:%.2e ", sum);
	}
    free(ht);
    LOG(dbg_lvl, "%s", str);
    free(str);
	sid_w2v_ppb_lock = 0;
}

real ModelTrainProgress(real *progress, int thread_num, int iter_num) {
  real p = NumSumVec(progress, thread_num);
  p /= (thread_num * iter_num);
  return p;
}

int getSentRange(FILE*fin, long int fbeg, long int fend){
	fseek(fin, fbeg, SEEK_SET);
	int scount = 0;
	int wnum = 0, wids[SUP];
	long int fpos = 0;
	while(1){
		wnum = TextReadSent(fin, vcb, wids, V_TEXT_LOWER, V_TEXT_RM_TRAIL_PUNC, 1);
		fpos = ftell(fin);
        if (wnum > 1)
			scount++;
		if (feof(fin) || fpos >= fend)
			break;

	}
	SENT_COUNT += scount;
	return scount;
}

void snnmfPrep(){
	snnmfInit();
}

real tau(real val, real threshold){
    real sign = (val>=0)?1:-1;
    val = fabs(val);
    if (val <= threshold)
        return 0;
    else
        return sign * (val - threshold);
}

void calDelta(real* Delta, int* wids, int wnum, real* Ti, real** Bi){
	memset(Delta, 0, vcb->size * sizeof(real));
	int * freq = (int *)malloc(vcb->size * sizeof(int));
	memset(freq, 0, vcb->size * sizeof(int));
	int r=0, j=0;
	for (r=0; r<wnum; r++){
		int wid = wids[r];
		freq[wid]++;
		if (freq[wid] > 1) continue;
		for (j=0; j<V_PHRASE_SIZE; j++)
			Delta[wid] += Ti[j] * Bi[wid][j];
	}
	for (r=0; r<wnum; r++)	Delta[wids[r]] -= freq[wids[r]];
	free(freq);
}

void updateT(real* Delta, int* wids, int wnum, real* Si, real* Ti, real gamma){
	int r=0, j=0;
	for (j=0; j<V_PHRASE_SIZE; j++){
		real d = 0;
		for (r=0; r<wnum; r++)  d += Delta[wids[r]] * B[wids[r]][j];
		real s1 = tau(Ti[j] - 2 * gd_ss * d, gd_ss * LAMBDA_S);
		Ti[j] = (1 - gamma) * s1 + gamma * Si[j];
		Si[j] = s1;
	}
}

void updateB(real* Delta, int* wids, int wnum, real *Si, real gamma){
	int r=0, j=0;
	for (r=0; r<wnum; r++){
		int wid = wids[r];
		for (j=0; j<V_PHRASE_SIZE; j++){
			int wid = wids[r];
			real d = Delta[wid] * Si[j];
			real b1 = tau(B[wid][j] - 2 * gd_ss * d, gd_ss * LAMBDA_B);
			C[wid][j] = (1 - gamma) * b1 + gamma * B[wid][j];
			B[wid][j] = b1;
		}
	}
}

void snnmfUpdate(int* wids, int wnum, real *Si, real *Ti, real gamma){
	real *Delta = (real *)malloc(vcb->size * sizeof(real)); 

	calDelta(Delta, wids, wnum, Ti, B);
	updateT(Delta, wids, wnum, Si, Ti, gamma);

	calDelta(Delta, wids, wnum, Si, C);
	updateB(Delta, wids, wnum, Si, gamma);

    free(Delta);
}

real getLoss(FILE* fin, real **S, long int fbeg, long int fend){
	fseek(fin, fbeg, SEEK_SET);
	int sindex = 0;
	int wids[SUP], wnum=0, i=0, r=0;
	long int fpos = fbeg;
	real *dot = (real *)malloc(vcb->size * sizeof(real));
	int *freq = (int *)malloc(vcb->size * sizeof(int));
	int *used = (int *)malloc(vcb->size * sizeof(int));
	memset(dot, 0, vcb->size * sizeof(real));
	memset(freq, 0, vcb->size * sizeof(int));
	memset(used, 0, vcb->size * sizeof(int));

	real l = 0;
	while (1){
		wnum = TextReadSent(fin, vcb, wids, V_TEXT_LOWER, V_TEXT_RM_TRAIL_PUNC, 1);
		if (wnum <=1 )
			continue;
		for (r=0; r<wnum; r++){
			int wid = wids[r];
			freq[wid]++;
			if (freq[wid] > 1)	continue;
			for (i=0; i<V_PHRASE_SIZE; i++)
				dot[wid] += B[wid][i] * S[sindex][i];
		}
		for (r=0; r<wnum; r++){
			int wid = wids[r];
			if (used[wid])	continue;
			used[wid] = 1;
			dot[wid] -= freq[wid];
			l += dot[wid] * dot[wid];
		}
		for (r=0; r<wnum; r++){
			int wid = wids[r];
			used[wid] = freq[wid] = 0;
			dot[wid] = 0;
		}
		sindex++;
		fpos = ftell(fin);
		if (feof(fin) || fpos >= fend)
			break;
    }
	free(dot);
	free(freq);
	free(used);
	return l / sindex;
}

void *snnmfThreadTrain(void *arg) {
    int tid = (long)arg;
	FILE *fin = fopen(V_TEXT_FILE_PATH, "rb");
	if (!fin){
		LOG(0, "Error!\n");
		exit(1);
	}
	fseek(fin, 0, SEEK_END);
	long int fsz = ftell(fin);
	long int fallbeg = fsz / V_THREAD_NUM * tid;
	long int fallend = fsz / V_THREAD_NUM * (tid + 1);
	int part_num = 0;
	for (part_num = 0; part_num < V_PARTITION_NUM; part_num++){
		long int fbeg = fallbeg + (fallend - fallbeg) / V_PARTITION_NUM * part_num;
		long int fend = fallbeg + (fallend - fallbeg) / V_PARTITION_NUM * (part_num + 1);
		long int fpos = fbeg;
    	int slen  = getSentRange(fin, fbeg, fend);
		if (slen == 0)
			continue;
		fseek(fin, fbeg, SEEK_SET);
		int i=0, j=0, sindex = 0;
		int wids[SUP], wnum;
	    real ** S = (real**) malloc(slen * sizeof(real*));
	    real ** T = (real**) malloc(slen * sizeof(real*));
	    for (i=0; i<slen; i++){
	        S[i] = (real*) malloc(V_PHRASE_SIZE * sizeof(real));
	        T[i] = (real*) malloc(V_PHRASE_SIZE * sizeof(real));
    	    for (j=0; j<V_PHRASE_SIZE; j++){
	            S[i][j] = NumRand();
    	        T[i][j] = S[i][j];
        	}
	    }
    	real lambda_pre = 0;
		real lambda_cur = (1 + sqrt(1 + 4 * lambda_pre * lambda_pre)) / 2.0;
		real gamma = (1 - lambda_pre) / lambda_cur;
	    real p = 0, entp = 0;
		int iter_num = 0;
	    while (iter_num < V_ITER_NUM){
			wnum = TextReadSent(fin, vcb, wids, V_TEXT_LOWER, V_TEXT_RM_TRAIL_PUNC, 1);
			fpos = ftell(fin);
    	    if (wnum > 1){
				snnmfUpdate(wids, wnum, S[sindex], T[sindex], gamma);
				sindex++;
			}
			if (i++ >= 1000){
				i = 0;
        		progress[tid] = iter_num + (double)(fpos - fbeg) / (fend - fbeg);
				entprogress[tid] = (part_num + (iter_num + (double)(fpos - fbeg) /(fend - fbeg)) / (double)V_ITER_NUM) / (double) V_PARTITION_NUM;
	        	p = ModelTrainProgress(progress, V_THREAD_NUM, V_ITER_NUM);
    	    	gd_ss = V_INIT_GRAD_DESCENT_STEP_SIZE * (1 - p);
				entp = NumSumVec(entprogress, V_THREAD_NUM) / (double) V_THREAD_NUM;
				if (IS_PRINT_LOSS)	loss[tid] = getLoss(fin, S, fbeg, fend);
	        	W2vThreadPrintProgBar(2, tid, entp);
			}
			if (feof(fin) || fpos >= fend){
				fseek(fin, fbeg, SEEK_SET);
				lambda_pre = lambda_cur;
				lambda_cur = (1 + sqrt(1 + 4 * lambda_pre * lambda_pre)) / 2.0;
				gamma = (1- lambda_pre) / lambda_cur;
				sindex = 0;
				if (V_CACHE_INTERMEDIATE_MODEL && iter_num % V_CACHE_INTERMEDIATE_MODEL == 0)
						ModelSave(V_MODEL_SAVE_PATH);
				iter_num++;
			}	
    	}
		for (i=0; i<slen; i++){
			free(T[i]);
			free(S[i]);
		}
		free(T);
		free(S); 
	}
	fclose(fin);
	pthread_exit(NULL);
}

void FreeC(){
	int i =0;
    for (i=0; i<vcb->size; i++)
        free(C[i]);
    free(C); 
}

void snnmClean(){
	FreeC();
}
