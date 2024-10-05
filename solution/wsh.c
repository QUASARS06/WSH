#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include "wsh.h"

int history_capacity = HISTORY_SIZE;
int curr_history_size = 0;

HistNode *histHead = NULL;
HistNode *histTail = NULL;

LocalNode *localHead = NULL;

// variables used to store the command input by user
char cmd_buf[MAXLINE];
char *cmd_args[MAXARGS];

// stores the last command issued by the user
char last_command[MAXLINE];

// redirection
bool redirect_append = false;

bool redirect_in = false;
bool redirect_out = false;
bool redirect_err = false;

int orig_stdin;
int orig_stdout;
int orig_stderr;

char *redirect_filename = NULL;
int redirect_fd = -1;

// error executing cmds
bool is_err = false;


void free_memory(void) {
    // Free History
    HistNode *histPtr = histHead;
    while(histPtr != NULL) {
        HistNode *h = histPtr;
        histPtr = histPtr->next;
        free(h);
    }

    // Free Local
    LocalNode *localPtr = localHead;
    while(localPtr != NULL) {
        LocalNode *l = localPtr;
        localPtr = localPtr->next;
        free(l);
    }
}


/**
 * Counts number of args in cmd_args
 * Ignore the first item which is the actual command
 */
int count_cmd_args(void) {
    int cmd_args_count = 0;
    for(int i = 1 ; cmd_args[i] != NULL ; i++) {
        cmd_args_count++;
    }

    return cmd_args_count;
}

/**
 * If there are any variables in the args list then we replace it by their corresponding value
 */
int replace_vars(void) {

    for(int i = 0 ; cmd_args[i] != NULL ; i++) {
        if(cmd_args[i][0] == '$') {
            
            char *var_name = cmd_args[i] + 1;
            char *var_val;

            if(getenv(var_name) != NULL) {
                var_val = getenv(var_name);
            } 
            else if(searchLocal(var_name) != NULL) {
                var_val = searchLocal(var_name);
            }
            else {
                var_val = "";
            }
            cmd_args[i] = strdup(var_val);
        }
    }

    return 0;
}

/**
 * Reads input from stdin and then stores it in the cmd_buf passed
 */
int read_cmd(char *cmd, size_t cmd_sz) {
    printf("wsh> ");
    fflush(stdout);

    int cmdLength = getline(&cmd, &cmd_sz, stdin);
    if(cmdLength == -1) return -1;

    return 0;
}

int set_redirection(void) {    
    if(redirect_out) {
        int output_fd = open(redirect_filename, O_WRONLY | O_CREAT | (redirect_append ? O_APPEND : O_TRUNC), 0644);
        if(output_fd < 0) return -1;

        if(redirect_err) dup2(output_fd, STDERR_FILENO);
        dup2(output_fd, redirect_fd);

        close(output_fd);
    }

    if(redirect_in) {
        int input_fd = open(redirect_filename, O_RDONLY);
        if(input_fd < 0) return -1;

        dup2(input_fd, redirect_fd);
        
        close(input_fd);
    }

    return 0;

}

void unset_redirection(void) {
    dup2(orig_stdin, STDIN_FILENO);
    dup2(orig_stdout, STDOUT_FILENO);
    dup2(orig_stderr, STDERR_FILENO);
    close(orig_stdin);
    close(orig_stdout);
    close(orig_stderr);
}

void check_redirection(char *token) {

    if(strstr(token, "&>>") != NULL) {
        redirect_out = true;
        redirect_err = true;
        redirect_append = true;

        redirect_fd = 1;
        redirect_filename = strtok(token, "&>>");
    } 
    else if(strstr(token, "&>") != NULL) {
        redirect_out = true;
        redirect_err = true;

        redirect_fd = 1;
        redirect_filename = strtok(token, "&>");
    }  
    else if(strstr(token, ">>") != NULL) {
        redirect_out = true;
        redirect_append = true;
        
        if(isdigit(token[0])) {
            redirect_fd = atoi(strtok(token, ">>"));
            redirect_filename = strtok(NULL, "");
        } else {
            redirect_fd = 1;
            redirect_filename = strtok(token, ">>");
        }
    }
    else if(strstr(token, ">") != NULL) {
        redirect_out = true;

        if(isdigit(token[0])) {
            redirect_fd = atoi(strtok(token, ">"));
            redirect_filename = strtok(NULL, "");
        } else {
            redirect_fd = 1;
            redirect_filename = strtok(token, ">");
        }
    }
    else if(strstr(token, "<") != NULL) {
        redirect_in = true;

        if(isdigit(token[0])) {
            redirect_fd = atoi(strtok(token, "<"));
            redirect_filename = strtok(NULL, "");
        } else {
            redirect_fd = 0;
            redirect_filename = strtok(token, "<");
        }
    }

    // printf("Redirect FD = %d\n", redirect_fd);
    // printf("Filename = %s\n", filename);
    // printf("IN = %d | OUT = %d | ERR = %d | APPEND = %d\n\n", redirect_in, redirect_out, redirect_err, redirect_append);

}

/**
 * Parses the cmd_buf string and breaks it into tokens separated by " "
 * The tokens are then saved in the cmg_args_list array
 */
int parse_cmd(char *cmd_buf_to_parse) {

    // redirection
    redirect_filename = NULL;
    redirect_fd = -1;

    redirect_append = false;

    redirect_in = false;
    redirect_out = false;
    redirect_err = false;

    orig_stdin = dup(STDIN_FILENO);
    orig_stdout = dup(STDOUT_FILENO);
    orig_stderr = dup(STDERR_FILENO);

    char *token;

    // first token
    token = strtok(cmd_buf_to_parse, " ");

    int i = 0;
    while(token != NULL) {
        // if we encounter a '#' at the start of any token we stop processing the rest of the input sequence
        if(strlen(token) >= 1 && token[0] == '#') break;

        check_redirection(token);
        if(redirect_in || redirect_out || redirect_err) break;
        
        cmd_args[i] = token;
        i++;
        token = strtok(NULL, " ");
    }
    cmd_args[i] = NULL;

    // replace variables by values
    replace_vars();

    if(strcmp(cmd_args[0], "exit") != 0) {
        //err
        is_err = false;
    }

    return 0;
}

/**
 * Prints the history pointed by HEAD
 * The history LL ends with a NULL so it's prints till NULL is encoutered
 */
void printHistory(void) {
    HistNode *ptr = histHead;
    
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
char * searchHistory(int target_idx) {
    HistNode *ptr = histHead;
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
void addToHistory(void) {

    HistNode *NN = (HistNode*) malloc(sizeof(HistNode));
    NN->prev = NULL;
    NN->next = NULL;
    strcpy(NN->cmd, cmd_buf);

    if(histHead == NULL && histTail == NULL) {
        histHead = NN;
        histTail = NN;
        curr_history_size += 1;
        return;
    }

    histHead->prev = NN;
    NN->next = histHead;
    histHead = NN;

    if(curr_history_size >= history_capacity) {
        HistNode *t = histTail;
        histTail = histTail->prev;
        histTail->next = NULL;

        free(t);
    } else {
        curr_history_size += 1;
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
void updateHistoryCapacity(int new_hist_capacity) {
    
    if(new_hist_capacity == 0) {
        histHead = NULL;
        histTail = NULL;
        history_capacity = new_hist_capacity;
        curr_history_size = 0;
    }
    
    if(new_hist_capacity < history_capacity && curr_history_size > new_hist_capacity) {

        int nodesToDelete = curr_history_size - new_hist_capacity;

        while(nodesToDelete > 0) {
            HistNode *t = histTail;
            histTail = histTail->prev;
            histTail->next = NULL;

            free(t);
            nodesToDelete -= 1;
        }
        curr_history_size = new_hist_capacity;
    }
    
    history_capacity = new_hist_capacity;
}

/**
 * Executes the history command
 * 1) history - prints the history
 * 2) history set n - updates history capactiy to n
 * 3) history n - executes the nth command in the history
 */
int history(bool *is_from_history) {

    // Built-In history command
    if(cmd_args[1] == NULL) {
        // print the history
        printHistory();
    }

    // if history set # command is executed - update history size
    else if(strcmp(cmd_args[1], "set") == 0) {
        int new_hist_capactiy = atoi(cmd_args[2]);
        if(new_hist_capactiy > 0) {
            updateHistoryCapacity(new_hist_capactiy);
        }
    }

    else {
        // check cmd_args[1] is a valid integer
        *is_from_history = true;
        int hist_idx = atoi(cmd_args[1]);
        if(hist_idx > 0 && hist_idx <= curr_history_size) {
            char *hist_cmd = searchHistory(hist_idx);
            parse_cmd(hist_cmd);
            run_command();
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
int cd(void) {
    if(cmd_args[1] == NULL) return -1;

    if(count_cmd_args() != 1) {
        is_err = true;
        return -1;
    }
    
    if(chdir(cmd_args[1]) != 0) {
        is_err = true;
        return -1;
    }

    return 0;
}

/**
 * Gets cmd args in the form varname=varvalue
 * Sets this in the current processes environment variable
 */
int export(void) {
    if(count_cmd_args() != 1) {
        return -1;
    }
    putenv(cmd_args[1]);
    return 0;
}

/**
 * Prints the local variables LL pointed by localHead
 */
int vars(void) {
    if(localHead == NULL) return 0;
    LocalNode *ptr = localHead;
    
    while(ptr != NULL) {
        if(ptr->varvalue != NULL && strlen(ptr->varvalue) > 0) printf("%s=%s\n", ptr->varname, ptr->varvalue);
        ptr = ptr->next;
    }

    return 0;
}

/**
 * Checks for a given index value in the History LL
 */
char * searchLocal(char* varname) {
    LocalNode *ptr = localHead;
    while(ptr != NULL) {
        if(strcmp(varname, ptr->varname) == 0) return ptr->varvalue;
        ptr = ptr->next;
    }

    return NULL;
}

int local(void) {
    if(count_cmd_args() != 1) return -1;

    char *token;
    char *varname = NULL;
    char *varvalue = NULL;

    // first token
    token = strtok(cmd_args[1], "=");

    int ct = 0;
    while(token != NULL) {
        ct++;
        if(ct == 1) varname = token;
        else if(ct == 2) varvalue = token;
        else return -1;

        token = strtok(NULL, " ");
    }

    LocalNode *ptr = localHead;

    while(ptr != NULL && ptr->next != NULL) {
        if(strcmp(ptr->varname, varname) == 0) break;
        ptr = ptr->next;
    }

    if(ptr == NULL || ptr->next == NULL) {
        LocalNode *LN = (LocalNode*) malloc(sizeof(LocalNode));
        
        LN->varname = varname;
        LN->varvalue = varvalue;
        LN->next = NULL;

        if(ptr == NULL) localHead = LN;
        else {
            ptr->next = LN;
        }
    } else {
        ptr->varvalue = varvalue;
    }

    free(token);

    return 0;
}


int run_command(void) {
    char cmd_path[4096] = "";
    if(access(cmd_args[0], X_OK) == 0) {
        strcpy(cmd_path, cmd_args[0]);
    } else {
        char *path_original = getenv("PATH");
        char *path = strdup(path_original);

        char *token = strtok(path, ":");
        
        while(token != NULL) {
            snprintf(cmd_path, sizeof(cmd_path), "%s/%s", token, cmd_args[0]);
            if(access(cmd_path, X_OK) == 0) break;

            token = strtok(NULL, ":");
        }

        free(path);
    }
    
    if(strlen(cmd_path) == 0) {
        is_err = true;
        return -1;
    }

    pid_t pid = fork();
    
    if(pid < 0) {
        is_err = true;
        return -1;
    }
    else if(pid == 0) {
        // child process where we execute the command
        execv(cmd_path, cmd_args);
        
        // if execv returned it means some error
        exit(-1);
    } else {
        int status_ptr;
        waitpid(pid, &status_ptr, 0);
    
        if(WIFEXITED(status_ptr)) {
            int exit_status = WEXITSTATUS(status_ptr);
            if(exit_status != 0) {
                is_err = true;
                return -1;
            }
        } else {
            is_err = true;
            return -1;
        }
    }

    return 0;
}

int exec_cmd(void) {

    if(redirect_in || redirect_out || redirect_err) {
        set_redirection();
    }

    bool is_from_history = false;   // stores whether a NON built-in command is requested via history or not
    bool is_built_in = true;
    
    if(strcmp(cmd_args[0], "exit") == 0) {      // if the command passed is exit then exit(-1) gracefully
        free_memory();
        exit(is_err ? -1 : 0);
    }
    else if(strcmp(cmd_args[0], "cd") == 0) {    // Built-In change directory
        cd();
    }
    else if(strcmp(cmd_args[0], "export") == 0) {   // Built-In export 'GLOBAL' variables
        export();
    }
    else if(strcmp(cmd_args[0], "local") == 0) {    // Built-In local variables
        local();  
    }
    else if(strcmp(cmd_args[0], "vars") == 0) { // Built-In list all the variables
        vars();   
    }
    else if(strcmp(cmd_args[0], "history") == 0) {  // Built-In history command
        history(&is_from_history);
    }
    else if(strcmp(cmd_args[0], "ls") == 0) {   // Built-In ls -1 command
        ls();
    }
    else {
        is_built_in = false;
    }

    // since it's not a built-in command it will be saved in the history
    if(!is_from_history && !is_built_in && !(strcmp(last_command, cmd_buf) == 0)) {
        addToHistory();
    }

    // we set the current command being parsed always so that we can use it to update the history quickly
    if(!is_built_in) {
        // fork and execute in child process
        run_command();
        strcpy(last_command, cmd_buf);
    }

    if(redirect_in || redirect_out || redirect_err) {
        unset_redirection();
    }

    return 0;
}

int run_batch_mode(char *file_name) {
    FILE *batch_file = fopen(file_name, "r");
    if(batch_file == NULL) {
        exit(-1);
    }

    char *line = NULL;
    size_t line_buf_sz = 0;
    ssize_t line_length = 0;

    while((line_length = getline(&line, &line_buf_sz, batch_file)) != -1) {
        if(line_length <= 1 || line[0] == '#') continue;
        if(line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';

        // parse the input command buffer to tokenize and store in the array
        char* cmd_buf_cpy = strdup(line);
        parse_cmd(cmd_buf_cpy);

        if(cmd_args[0] == NULL) continue;

        // check if built-in
        exec_cmd();
    }

    fclose(batch_file);
    free(line);

    return 0;
}

int main(int argc, char* argv[]) {

    if(argc == 2) {
        // Batch Mode
        return run_batch_mode(argv[1]);
    } else if(argc > 2) {
        printf("Something is wrong\n");
    }

    while(read_cmd(cmd_buf, sizeof(cmd_buf)) >= 0) {
        
        if(cmd_buf[strlen(cmd_buf) - 1] == EOF) exit(0);

        if(cmd_buf[strlen(cmd_buf) - 1] == '\n') {
            cmd_buf[strlen(cmd_buf) - 1] = '\0';
        }

        // parse the input command buffer to tokenize and store in the array
        char* cmd_buf_cpy = strdup(cmd_buf);
        parse_cmd(cmd_buf_cpy);

        if(cmd_args[0] == NULL) continue;

        // check if built-in
        exec_cmd();
    }
    
    free_memory();
    return is_err ? -1 : 0;
}
