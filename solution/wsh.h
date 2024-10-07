#define HISTORY_SIZE 5      // Initial size of History
#define MAXLINE 1024        // Maximum length of input to wsh i.e. a single command
#define MAXARGS 128         // Maximum number of arguments to parse for the input command cp {-r -s -t} => 3

typedef struct HistNode {
    char cmd[MAXLINE];
    struct HistNode *next;
    struct HistNode *prev;
} HistNode;

typedef struct LocalNode {
    char *varname;
    char *varvalue;
    struct LocalNode *next;
} LocalNode;



int run_command(void);
char * searchLocal(char*);



int read_cmd(char *, size_t);
int parse_cmd(char *);
int exec_cmd(void);
int run_batch_mode(char *);
