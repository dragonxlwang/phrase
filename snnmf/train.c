#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../snnmf/variables.c"
#include "../snnmf/snnmf.c"
#include "../snnmf/constants.c" 
#include "../utils/util_misc.c"
#include "../utils/util_num.c"
#include "../utils/util_text.c"

void PhraseSave(char *fp){
	FILE *fout = fopen(fp, "w");
	if (!fout){
		LOG(0, "Error!\n");
		exit(1);
	}
	int i=0, j=0;
	for (i=0; i<V_PHRASE_SIZE; i++){
		pair *pairs = (pair *) malloc(vcb->size * sizeof(pair));
		for (j=0; j<vcb->size; j++){
			pairs[j].key = j;
			pairs[j].val = B[j][i];
		}
		pairs = sort_tuples(pairs, vcb->size, 1);
		for (j=0; j<V_TOP_WORD_NUM_IN_PHRASE; j++){
			if (pairs[j].val == 0)
				break;
			if (j > 0)	fprintf(fout, "|");
			fprintf(fout, "%s:%.6e", vcb->id2wd[pairs[j].key], pairs[j].val);
		}
		fprintf(fout, "\n");
		free(pairs);
	}
	fclose(fout);
}

void ModelSave(char *fp){
	FILE *fout = fopen(fp, "wb");
	if (!fout){
		LOG(0, "Error!\n");
		exit(1);
	}
	fwrite(&vcb->size, sizeof(int), 1, fout);
	fwrite(&V_PHRASE_SIZE, sizeof(int), 1, fout);
	fwrite(B, sizeof(real), vcb->size * V_PHRASE_SIZE, fout);
	fclose(fout);
	LOGC(1, 'c', 'k', "[MODEL]: Save model to %s\n", fp);
}

void Train(int argc, char* argv[]) {
	NumInit();
	VariableInit(argc, argv);  //  >>
	MainWork prepper = NULL, cleaner = NULL;
	ThreadTrain trainer = NULL;
	prepper = snnmfPrep;
	trainer = snnmfThreadTrain;
	cleaner = snnmClean;
	if (prepper) prepper();
	if (trainer) {
		int tid;
	    // schedule thread jobs
		pthread_t* pt = (pthread_t*)malloc(V_THREAD_NUM * sizeof(pthread_t));
		for (tid = 0; tid < V_THREAD_NUM; tid++)
			pthread_create(&pt[tid], NULL, trainer, (void*)tid);
		for (tid = 0; tid < V_THREAD_NUM; tid++)
			pthread_join(pt[tid], NULL);
		free(pt);
	}
	if (cleaner) cleaner();
	LOG(1, "\nTraining finished. Took time %s\n", strclock(start_clock_t, clock(), V_THREAD_NUM));
	ModelSave(V_MODEL_SAVE_PATH); // save output
	PhraseSave(V_PHRASE_SAVE_PATH);
	VariableFree();
	LOG(1, "\nOutput finished. Took time %s\n", strclock(start_clock_t, clock(), V_THREAD_NUM));
}

int main(int argc, char* argv[]) {
	Train(argc, argv);
	return 0;
}
