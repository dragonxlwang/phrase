#ifndef CONSTANTS
#define CONSTANTS

// The following parameters are set in order to have variables on stack
#define NUP 200                // upper bound for N  (embedding dimension)
#define VUP 0xFFFFF            // upper bound for V  (vocabulary)
#define PUP 10                 // upper bound for phrase length
#define WUP TEXT_MAX_WORD_LEN  // upper bound for character number in a word
#define SUP TEXT_MAX_SENT_WCT  // upper bound for word number in a sentence

#endif /* ifndef CONSTANTS */
