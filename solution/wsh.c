#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include "wsh.h"

/**
 * Counts number of args in cmd_args
 * Ignore the first item which is the actual command
 */
int count_cmd_args(char *cmd_args[]) {
    int cmd_args_count = 0;
    for(int i = 1 ; cmd_args[i] != NULL ; i++) {
        cmd_args_count++;
    }

    return cmd_args_count;
}

int set_local_vars_file_name(char **local_vars_file_name) {
    char *s1 = "local";
    char *s2 = ".txt";

    pid_t pid = getpid();
    char *pid_str = NULL;

    if(asprintf(&pid_str, "%jd", (intmax_t) pid) == -1) return -1;
    
    *local_vars_file_name = malloc(strlen(s1) + strlen(s2) + strlen(pid_str) + 1);

    strcpy(*local_vars_file_name, s1);
    strcat(*local_vars_file_name, pid_str);
    strcat(*local_vars_file_name, s2);

    return 0;
}

/**
 * Reads input from stdin and then stores it in the cmd_buf passed
 */
int read_cmd(char *cmd_buf, size_t cmd_buf_sz) {
    printf("wsh> ");

    int cmdLength = getline(&cmd_buf, &cmd_buf_sz, stdin);
    if(cmdLength == -1) return -1;

    return 0;
}

/**
 * If there are any variables in the args list then we replace it by their corresponding value
 */
int replace_vars(char *cmd_buf[]) {

    for(int i = 0 ; cmd_buf[i] != NULL ; i++) {
        if(cmd_buf[i][0] == '$') {
            
            char *var_name = cmd_buf[i] + 1;
            char *var_val;

            if(getenv(var_name) != NULL) {
                var_val = getenv(var_name);
            } else {
                var_val = "";
            }

            cmd_buf[i] = strdup(var_val);
        }
    }

    return 0;
}

/**
 * Parses the cmd_buf string and breaks it into tokens separated by " "
 * The tokens are then saved in the cmg_args_list array
 */
int parse_cmd(char *cmd_buf, char *cmd_args_list[]) {
    char *token;

    // first token
    token = strtok(cmd_buf, " ");

    int i = 0;
    while(token != NULL) {
        cmd_args_list[i] = token;
        i++;
        token = strtok(NULL, " ");
    }
    cmd_args_list[i] = NULL;

    // replace variables by values
    replace_vars(cmd_args_list);

    return 0;
}

/**
 * Prints the history pointed by HEAD
 * The history LL ends with a NULL so it's prints till NULL is encoutered
 */
void printHistory(HistNode *HEAD) {
    HistNode *ptr = HEAD;
    
    int i = 1;
    while(ptr != NULL) {
        printf("%d) %s\n", i, ptr->cmd);
        i++;
        ptr = ptr->next;
    }
}

/**
 * Checks for a given index value in the History LL
 */
char * searchHistory(HistNode *HEAD, int target_idx) {
    HistNode *ptr = HEAD;
    while(target_idx > 1) {
        ptr = ptr->next;
        target_idx--;
    }

    return ptr->cmd;
}

/**
 * Adds a NON built-in and NON history executed command into the History
 * If overflow then truncate old commands in the history
 */
void addToHistory(HistNode **HEAD, HistNode **TAIL, char* cmd, int *curr_hist_size, int hist_capacity) {

    HistNode *NN = (HistNode*) malloc(sizeof(HistNode));
    NN->prev = NULL;
    NN->next = NULL;
    strcpy(NN->cmd, cmd);

    if(*HEAD == NULL && *TAIL == NULL) {
        *HEAD = NN;
        *TAIL = NN;
        *curr_hist_size += 1;
        return;
    }

    (*HEAD)->prev = NN;
    NN->next = *HEAD;
    *HEAD = NN;

    if(*curr_hist_size >= hist_capacity) {
        HistNode *t = *TAIL;
        *TAIL = (*TAIL)->prev;
        (*TAIL)->next = NULL;

        free(t);
    } else {
        *curr_hist_size += 1;
    }

}

/**
 * Update the history capacity to new_hist_capacity
 * (I) if we are shrinking the capacity then we need to think about different cases
 * Case 1: curr_hist_size >= new_hist_size - means we have more history then we can fit so delete
   in this case delete curr_hist_size - new_hist_capacity nodes from the end
 * 
 * Case 2: curr_hist_size < new_hist_size
   in this case we don't do anything, just simply update the hist_capacticy size
 * 
 * (II) if the new capacity is greater than the old capacity then just update the hist_capacity variable
 * 
 * (III) if the new and old hist capacities are the same then don't do anything
 */
void updateHistoryCapacity(HistNode **HEAD, HistNode **TAIL, int *curr_hist_size, int *old_hist_capacity, int new_hist_capacity) {
    
    if(new_hist_capacity == 0) {
        *HEAD = NULL;
        *TAIL = NULL;
        *old_hist_capacity = new_hist_capacity;
        *curr_hist_size = 0;
    }
    
    if(new_hist_capacity < *old_hist_capacity && *curr_hist_size > new_hist_capacity) {

        int nodesToDelete = *curr_hist_size - new_hist_capacity;

        while(nodesToDelete > 0) {
            HistNode *t = *TAIL;
            *TAIL = (*TAIL)->prev;
            (*TAIL)->next = NULL;

            free(t);
            nodesToDelete -= 1;
        }
        *curr_hist_size = new_hist_capacity;
    }
    
    *old_hist_capacity = new_hist_capacity;
}

/**
 * Executes the history command
 * 1) history - prints the history
 * 2) history set n - updates history capactiy to n
 * 3) history n - executes the nth command in the history
 */
int history(char *cmd_args[], HistNode **histHead, HistNode **histTail, bool *is_from_history, int *curr_hist_size, int *hist_capacity) {

    // Built-In history command
    if(cmd_args[1] == NULL) {
        // print the history
        printHistory(*histHead);
    }

    // if history set # command is executed - update history size
    else if(strcmp(cmd_args[1], "set") == 0) {
        int new_hist_capactiy = atoi(cmd_args[2]);
        if(new_hist_capactiy > 0) {
            updateHistoryCapacity(histHead, histTail, curr_hist_size, hist_capacity, new_hist_capactiy);
        }
    }

    else {
        // check cmd_args[1] is a valid integer
        *is_from_history = true;
        int hist_idx = atoi(cmd_args[1]);
        if(hist_idx > 0 && hist_idx <= *curr_hist_size) {
            char *hist_cmd = searchHistory(*histHead, hist_idx);
            printf("%s\n", hist_cmd);
        }
    }

    return 0;
}

/**
 * Custom Implementation of ls -1 command
 */
int ls(void) {
    char pwd[PATH_MAX];
    if(getcwd(pwd, sizeof(pwd)) == NULL) {
        return -1;
    }
    
    struct dirent **allFileNames; 
    int n = scandir(pwd, &allFileNames, NULL, alphasort);

    if(n < 0) {
        perror("ls error");
    } else {  
        int i = 0;  
        while(i < n) {
            if(allFileNames[i]->d_name[0] != '.') {
                printf("%s\n", allFileNames[i]->d_name);
            }
            free(allFileNames[i]);
            i++;
        }
        free(allFileNames);
    }

    return 0;
}


/**
 * Custom implementation of cd command
 */
int cd(char *cmd_args[]) {
    if(cmd_args[1] == NULL) return -1;

    if(count_cmd_args(cmd_args) != 1) return -1;
    
    if(chdir(cmd_args[1]) != 0) {
        perror("chdir failed\n");
    }

    return 0;
}

/**
 * Gets cmd args in the form varname=varvalue
 * Sets this in the current processes environment variable
 */
int export(char* cmd_args[]) {
    if(count_cmd_args(cmd_args) != 1) return -1;
    putenv(cmd_args[1]);
    return 0;
}

int local(char *cmd_args[], char *LOCAL_VARS) {
    if(count_cmd_args(cmd_args) != 1) return -1;

    FILE *fp = fopen(LOCAL_VARS, "ab+");
    
    fclose(fp);

    return 0;
}

int run_command(char *cmd, char *cmd_args[]) {
    (void)cmd_args;
    (void)cmd;
    char *path = getenv("PATH");
    printf("PATH = |%s|\n", path);
    return 0;
}


int exec_cmd(char *cmd, char *cmd_args[], HistNode **histHead, HistNode **histTail, int *curr_hist_size, int *hist_capacity, char *last_command, char *LOCAL_VARS) {

    bool is_from_history = false;   // stores whether a NON built-in command is requested via history or not
    bool is_built_in = true;

    if(strcmp(cmd_args[0], "cd") == 0) {
        // Built-In change directory
        cd(cmd_args);
    }
    else if(strcmp(cmd_args[0], "export") == 0) {
        // Built-In export 'GLOBAL' variables
        export(cmd_args);

    }
    else if(strcmp(cmd_args[0], "local") == 0) {
        // Built-In local variables
        local(cmd_args, LOCAL_VARS);
        
    }
    else if(strcmp(cmd_args[0], "vars") == 0) {
        // Built-In list all the variables
        
    }
    else if(strcmp(cmd_args[0], "history") == 0) {
        // Built-In history command
        history(cmd_args, histHead, histTail, &is_from_history, curr_hist_size, hist_capacity);
    }
    else if(strcmp(cmd_args[0], "ls") == 0) {
        // Built-In ls -1 command
        ls();
    }
    else {
        is_built_in = false;
        // fork and execute in child process
    }

    // since it's not a built-in command it will be saved in the history
    if(!is_from_history && !is_built_in && !(last_command != NULL && strcmp(last_command, cmd) == 0)) {
        addToHistory(histHead, histTail, cmd, curr_hist_size, *hist_capacity);
    }

    // we set the current command being parsed always so that we can use it to update the history quickly
    if(!is_built_in) {

        //run_command(cmd, cmd_args);

        strcpy(last_command, cmd);
    }

    return 0;
}

int main(int argc, char* argv[]) {

    if(argc == 2) {
        // Batch Mode
        printf("Need to run %s\n", argv[1]);
    } else if(argc > 2) {
        printf("Something is wrong\n");
    }

    // Define all the variables
    // char *CURR_WORKING_DIR = NULL;
    // if(getcwd(CURR_WORKING_DIR, sizeof(CURR_WORKING_DIR)) == NULL) return -1;

    char* LOCAL_VARS = NULL;
    set_local_vars_file_name(&LOCAL_VARS);

    int history_capacity = HISTORY_SIZE;
    int curr_history_size = 0;
    HistNode *histHead = NULL;
    HistNode *histTail = NULL;

    // variables used to store the command input by user
    char cmd_buf[MAXLINE];
    char *cmd_args_list[MAXARGS];

    // stores the last command issued by the user
    char last_command[MAXLINE];

    while(read_cmd(cmd_buf, sizeof(cmd_buf)) >= 0) {
        if(cmd_buf[strlen(cmd_buf) - 1] == EOF || cmd_buf[strlen(cmd_buf) - 1] == '\n') {
            cmd_buf[strlen(cmd_buf) - 1] = '\0';
        }

        // parse the input command buffer to tokenize and store in the array
        char* cmd_buf_cpy = strdup(cmd_buf);
        parse_cmd(cmd_buf_cpy, cmd_args_list);

        // if the command passed is exit then exit(0) gracefully
        if(strcmp(cmd_args_list[0], "exit") == 0) exit(0);

        // check if built-in
        exec_cmd(cmd_buf, cmd_args_list, &histHead, &histTail, &curr_history_size, &history_capacity, last_command, LOCAL_VARS);

    }
    
    free(LOCAL_VARS);
    return 0;
}
