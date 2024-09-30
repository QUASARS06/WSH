#define HISTORY_SIZE 5      // Initial size of History
#define MAXLINE 1024        // Maximum length of input to wsh i.e. a single command
#define MAXARGS 128         // Maximum number of arguments to parse for the input command cp {-r -s -t} => 3

typedef struct HistNode {
    char cmd[MAXLINE];
    struct HistNode *next;
    struct HistNode *prev;
} HistNode;
