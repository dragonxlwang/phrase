#ifndef VARIABLES
#define VARIABLES

#include<math.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<unistd.h>
#include "../utils/util_misc.c"
#include "../utils/util_num.c"
#include "../utils/util_text.c"
#include "../snnmf/constants.c"

char *V_TEXT_FILE_PATH = "/home/sjiang18/data/gigaword_small/giga_nyt.txt";
char *V_VOCAB_FILE_PATH = NULL;
int V_TEXT_LOWER = 1;
int V_TEXT_RM_TRAIL_PUNC = 1;
char *V_MODEL_SAVE_PATH = NULL;
char *V_PHRASE_SAVE_PATH = "/home/sjiang18/data/gigaword_small/top_words_in_phrase.txt";
int V_TOP_WORD_NUM_IN_PHRASE = 10;
int V = 10000;
int V_VOCAB_HIGH_FREQ_CUTOFF = 80;
Vocabulary *vcb;
int SENT_COUNT = 0; //the number of sentences
real ** B = NULL; //B[i][j] weight of word i in phrase j
real LAMBDA_B = 1;
real LAMBDA_S = 1;
int V_PHRASE_SIZE = 100000;  //number of phrases
int V_THREAD_NUM = 20;
int V_PARTITION_NUM = 5;
int V_ITER_NUM = 1;
int V_CACHE_INTERMEDIATE_MODEL = 2;
real V_INIT_GRAD_DESCENT_STEP_SIZE = 1e-5;
real gd_ss;
real *progress;
real *entprogress;
int IS_PRINT_LOSS = 0;
real *loss = NULL;
clock_t start_clock_t;  // start time for model training

typedef void *(*ThreadTrain)(void *);
typedef void (*MainWork)();

void VariableInit(int argc, char ** argv){
    int i;
    char c = 'w';
    i = getoptpos("V_TEXT_LOWER", argc, argv);
    c = (i == -1)?'w':'r';
    if (i!=-1)  V_TEXT_LOWER=atoi(argv[i+1]);
    i = getoptpos("V_TEXT_RM_TRAIL_PUNC", argc, argv);
    if (c == 'w')   c = (i==-1)?'w':'r';
    if (i != -1)    V_TEXT_RM_TRAIL_PUNC = atoi(argv[i+1]);

    i = getoptpos("V_VOCAB_FILE_PATH", argc, argv);
    if(c == 'w')    c = (i==-1)?'w':'r';
    if (i!= -1) V_VOCAB_FILE_PATH = sclone(argv[i+1]);
    if (!V_VOCAB_FILE_PATH)
        V_VOCAB_FILE_PATH = FilePathSubExtension(
            V_TEXT_FILE_PATH,
            sformat("l%dr%d.vcb", V_TEXT_LOWER, V_TEXT_RM_TRAIL_PUNC));
    else
        V_VOCAB_FILE_PATH = FilePathExpand(V_VOCAB_FILE_PATH);
    LOGC(1, c, 'k', "Vocab File -------------------------------------- : %s\n", 
	     V_VOCAB_FILE_PATH);

    i = getoptpos("V_MODEL_SAVE_PATH", argc, argv);
    c = (i == -1)?'w':'r';
    if (i != -1)    V_MODEL_SAVE_PATH = sclone(argv[i+1]);
    if (!V_MODEL_SAVE_PATH)
        V_MODEL_SAVE_PATH = FilePathSubExtension(V_TEXT_FILE_PATH, "mdl");
    else
        V_MODEL_SAVE_PATH = FilePathExpand(V_MODEL_SAVE_PATH);
    LOGC(1, c, 'k', "Model File -------------------------------------- : %s\n", V_MODEL_SAVE_PATH);
    
    i = getoptpos("V", argc, argv);
    c = (i == -1)?'w':'r';
    if (i != -1) V = atoi(argv[i+1]);
    LOGC(1, c, 'k', "V -- vocabulary size cap ------------------------ : %d\n", V);

    i = getoptpos("V_VOCAB_HIGH_FREQ_CUTOFF", argc, argv);
    c = (i==-1)?'w':'r';
    if (i != -1)    V_VOCAB_HIGH_FREQ_CUTOFF = atoi(argv[i+1]);
    LOGC(1, c, 'k', "Vocabulary high-freq words cut-off -------------- : %d\n", V_VOCAB_HIGH_FREQ_CUTOFF);

	i = getoptpos("V_TEXT_LOWER", argc, argv);
	c = (i == -1) ? 'w' : 'r';
	if (i != -1) V_TEXT_LOWER = atoi(argv[i + 1]);
 	LOGC(1, c, 'k', "Convert uppercase to lowercase letters ---------- : %d\n",
         V_TEXT_LOWER);

	i = getoptpos("V_TEXT_RM_TRAIL_PUNC", argc, argv);
	c = (i == -1) ? 'w' : 'r';
	if (i != -1) V_TEXT_RM_TRAIL_PUNC = atoi(argv[i + 1]);
	LOGC(1, c, 'k', "Remove trailing punctuation --------------------- : %d\n",
         V_TEXT_RM_TRAIL_PUNC);

    //build vacobulary
    if (!fexists(V_VOCAB_FILE_PATH)){
        vcb = TextBuildVocab(V_TEXT_FILE_PATH, V_TEXT_LOWER, V_TEXT_RM_TRAIL_PUNC, -1);
        TextSaveVocab(V_VOCAB_FILE_PATH, vcb);
        VocabFree(vcb);
    }
    vcb = TextLoadVocab(V_VOCAB_FILE_PATH, V, V_VOCAB_HIGH_FREQ_CUTOFF);
    if (V != vcb->size){
        LOGC(0, 'r', 'k', "[VOCAB]: overwrite V from %d to %d\n", V, vcb->size);
        V = vcb->size;
    }

    i = getoptpos("V_THREAD_NUM", argc, argv);
    c = (i == -1) ? 'w' : 'r';
    if (i != -1) V_THREAD_NUM = atoi(argv[i + 1]);
    LOGC(1, c, 'k', "Thread Num -------------------------------------- : %d\n",
       V_THREAD_NUM);

    i = getoptpos("V_ITER_NUM", argc, argv);
    c = (i == -1) ? 'w' : 'r';
    if (i != -1) V_ITER_NUM = atoi(argv[i + 1]);
    LOGC(1, c, 'k', "Iterations -------------------------------------- : %d\n",
       V_ITER_NUM);

    i = getoptpos("V_INIT_GRAD_DESCENT_STEP_SIZE", argc, argv);
    c = (i == -1) ? 'w' : 'r';
    if (i != -1) V_INIT_GRAD_DESCENT_STEP_SIZE = atof(argv[i + 1]);
    LOGC(1, c, 'k', "Initial Grad Descent Step Size ------------------ : %lf\n",
       (double)V_INIT_GRAD_DESCENT_STEP_SIZE);
    gd_ss = V_INIT_GRAD_DESCENT_STEP_SIZE;

    progress = (real *) malloc(V_THREAD_NUM * sizeof(real));
	entprogress = (real *) malloc(V_THREAD_NUM * sizeof(real));
    start_clock_t = clock();

	loss = (real *)malloc(V_THREAD_NUM * sizeof(real)); 
	memset(loss, 0, V_THREAD_NUM * sizeof(real));
}

void VariableFree() {
	int i=0, j=0, k=0;
	for (i=0; i<vcb->size; i++)
		free(B[i]);
	free(B);
	VocabFree(vcb);
	free(progress);
	free(entprogress);
	free(loss);
}

#endif	//ifndef VARIABLES
