#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "wsh.h"

void printHistory(HistNode *HEAD) {
    
    HistNode *ptr = HEAD;
    
    int i = 1;
    while(ptr != NULL) {
        printf("%d) %s\n", i, ptr->cmd);
        i++;
        ptr = ptr->next;
    }
}

void addToHistory(HistNode **HEAD, HistNode **TAIL, char* cmd, int *curr_hist_size, int hist_capacity) {

    HistNode *NN = (HistNode*) malloc(sizeof(HistNode));
    NN->prev = NULL;
    NN->next = NULL;
    strcpy(NN->cmd, cmd);

    if(*HEAD == NULL && *TAIL == NULL) {
        *HEAD = NN;
        *TAIL = NN;
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

void updateHistoryCapacity(HistNode *HEAD, HistNode *TAIL, int *curr_hist_size, int *old_hist_capacity, int new_hist_capacity) {
    
    // (I) if we are shrinking the capacity then we need to think about different cases
    // Case 1: curr_hist_size >= new_hist_size - means we have more history then we can fit so delete
    // in this case delete curr_hist_size - new_hist_capacity nodes from the end

    // Case 2: curr_hist_size < new_hist_size
    // in this case we don't do anything, just simply update the hist_capacticy size

    
    // (II) if the new capacity is greater than the old capacity then just update the hist_capacity variable
    // (III) if the new and old hist capacities are the same then don't do anything
    (void)HEAD;
    if(new_hist_capacity == 0) {
        HEAD = NULL;
        TAIL = NULL;
        *old_hist_capacity = new_hist_capacity;
        *curr_hist_size = 0;
    }
    
    if(new_hist_capacity < *old_hist_capacity && *curr_hist_size > new_hist_capacity) {

        int nodesToDelete = *curr_hist_size - new_hist_capacity;

        while(nodesToDelete > 0) {
            HistNode *t = TAIL;
            TAIL = TAIL->prev;
            TAIL->next = NULL;

            free(t);
            nodesToDelete -= 1;
        }
        *curr_hist_size = new_hist_capacity;
    }
    
    *old_hist_capacity = new_hist_capacity;
}

int read_cmd(char *cmd_buf, size_t cmd_buf_sz) {
    printf("wsh> ");

    int cmdLength = getline(&cmd_buf, &cmd_buf_sz, stdin);
    if(cmdLength == -1) return -1;

    return 0;
}

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

    return 0;
}


int exec_cmd(char *cmd, char *cmd_args[], HistNode **histHead, HistNode **histTail, int *curr_hist_size, int *hist_capacity) {

    bool is_from_history = false;   // stores whether a NON built-in command is requested via history or not
    
    if(strcmp(cmd_args[0], "cd") == 0) {
        // Built-In change directory
    }
    else if(strcmp(cmd_args[0], "export") == 0) {
        // Built-In export 'GLOBAL' variables

    }
    else if(strcmp(cmd_args[0], "local") == 0) {
        // Built-In local variables
        
    }
    else if(strcmp(cmd_args[0], "vars") == 0) {
        // Built-In list all the variables
        
    }
    else if(strcmp(cmd_args[0], "history") == 0) {
        // Built-In history command
        if(cmd_args[1] == NULL) {
            // print the history
            printHistory(*histHead);
        }

        // if history # command is executed
        if(strcmp(cmd_args[1], "set") != 0) {
            is_from_history = true;
        }
    }
    else if(strcmp(cmd_args[0], "ls") == 0) {
        // Built-In ls -1 command
        
    }
    else {
        // fork and execute in child process
        if(!is_from_history) addToHistory(histHead, histTail, cmd, curr_hist_size, *hist_capacity);
        // since it's not a built-in command it will be saved in the history
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

    int history_capacity = HISTORY_SIZE;
    int curr_history_size = 0;
    HistNode *histHead = NULL;
    HistNode *histTail = NULL;

    // variables used to store the command input by user
    char cmd_buf[MAXLINE];
    char *cmd_args_list[MAXARGS];

    // stores the last command issued by the user
    char *last_command = NULL;

    while(read_cmd(cmd_buf, sizeof(cmd_buf)) >= 0) {
        if(cmd_buf[strlen(cmd_buf) - 1] == '\n') {
            cmd_buf[strlen(cmd_buf) - 1] = '\0';
        }

        // parse the input command buffer to tokenize and store in the array
        char* cmd_buf_cpy = strdup(cmd_buf);
        parse_cmd(cmd_buf_cpy, cmd_args_list);

        // if the command passed is exit then exit(0) gracefully
        if(strcmp(cmd_args_list[0], "exit") == 0) exit(0);


        // check if built-in
        exec_cmd(cmd_buf, cmd_args_list, &histHead, &histTail, &curr_history_size, &history_capacity);


        // we set the current command being parsed always so that we can use it to update the history quickly
        last_command = cmd_args_list[0];
    }

    // free(cmd_buf);
    // free(cmd_args_list);

    free(last_command);
    return 0;
}
