#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <signal.h>
#include "LineParser.h"
#include <stdlib.h>
#include <linux/limits.h>
#define RUNNING 1
#define SUSPENDED 0
#define TERMINATED -1
#define HISTLEN 20
#define BUFFER 2048

typedef struct process{
    cmdLine* parsedCmdLines; /* the parsed command line*/
    pid_t pid; /* the process id that is running the command*/
    int status; /* status of the process: RUNNING/SUSPENDED/TERMINATED */
    struct process *next; /* next process in chain */
} process;



process* myProcessList = NULL;
void freeProcessList(process* process_list){
    if(process_list){
        freeCmdLines(process_list->parsedCmdLines);
        free(process_list);
    }
}

void updateProcessList(process **process_list){
    int status;
    pid_t pid;
    while((pid = waitpid(-1, &status, WCONTINUED | WUNTRACED | WNOHANG)) > 0){
        while(*process_list){
            if((*process_list)->pid == pid){
                if(WIFSIGNALED(status) || WIFEXITED(status))
                    (*process_list)->status = TERMINATED;
                else if(WIFSTOPPED(status))
                    (*process_list)->status = SUSPENDED;
                else
                    (*process_list)->status = RUNNING;
            }
            process_list = &((*process_list)->next);
        }
    }
}





void addProcess(process** process_list, cmdLine* parsedCmdLines, pid_t pid){
    process* newProcess = malloc(sizeof(struct process));
    newProcess->pid = pid;
    newProcess->parsedCmdLines = parsedCmdLines;
    newProcess->next = process_list[0];
    process_list[0] = newProcess;
    if(newProcess->parsedCmdLines->next + newProcess->parsedCmdLines->blocking > 0) 
        newProcess->status = TERMINATED;
    else 
        newProcess->status = RUNNING;
}

void removeTerminatedProcesses(){
    process* temp = myProcessList;
    process* prev = NULL;
    while(temp != NULL){
        if(temp->status == TERMINATED){
            if(prev == NULL){
                myProcessList = myProcessList->next;
                freeProcessList(temp);
                temp = myProcessList;
            } else {
                prev->next = temp->next;
                freeProcessList(temp);
                temp = prev->next;
            }
        } else {
            prev = temp;
            temp = temp->next;
        }
    }
}

void printProcessList(process** process_list){
    updateProcessList(&myProcessList);

    printf("index          PID          Command         STATUS\n");
    process* newProcess = process_list[0];
    int cnt = 0;

    while(newProcess != NULL){
        char* status = "TERMINATED";
        if(newProcess->status == RUNNING){
            status = "RUNNING";
        }
        if(newProcess->status == SUSPENDED){
            status = "SUSPENDED";
        }

        printf("%d          %d          %s         %s\n", cnt, newProcess->pid, newProcess->parsedCmdLines->arguments[0], status);
        newProcess = newProcess->next;
        cnt++;
    }
    removeTerminatedProcesses();
}


int debugMode = 0;

void process_pipe(cmdLine* cmd1, cmdLine* cmd2){
    if(cmd1->outputRedirect){
        fprintf(stderr, "invalid output redirect in cmd1\n");
        return;
    }
    if(cmd2->inputRedirect){
        fprintf(stderr, "invalid userInput redirect in cmd2\n");
        return;
    }
    int pipefd[2];
    //step: 1  
    if(pipe(pipefd) < 0){
        perror("pipe");
        return;
    }
    //step: 2
    if(debugMode){ 
        fprintf(stderr, "(parent_process>forking...)\n");
    }
    int pid1 = fork();
    if(pid1 == 0){
        if(cmd1->inputRedirect){
            fclose(stdin);
            if(fopen(cmd1->inputRedirect, "r") == NULL){
                perror("fopen");
                _exit(EXIT_FAILURE);
            }
        }
        //step: 3
        //step: 3.1
        if(debugMode) {
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe...)\n");
        }
        close(STDOUT_FILENO);
        //step: 3.2
        dup(pipefd[1]);
        //step: 3.3
        close(pipefd[1]);
        //step: 3.4
        char* const* arg = cmd1->arguments;
        if(debugMode) {
        fprintf(stderr, "(child1>going to execute parsedCmdLines: %s)\n", arg[0]);
        }
        execvp(arg[0], arg);
        perror("execvp");
        _exit(EXIT_FAILURE);
    } 
    if(debugMode){
        fprintf(stderr, "(parent_process>created process with id: %d)\n", pid1);
    }

    if(debugMode){ 
    fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");
    }
    //step: 4
    close(pipefd[1]);
    //step: 5
    if(debugMode){ 
        fprintf(stderr, "(parent_process>forking...)\n");
    }

    int pid2 = fork();
    if(pid2 == 0){
        if(cmd2->outputRedirect){
            fclose(stdout);
            if(fopen(cmd2->outputRedirect, "w") == NULL){
                perror("fopen");
                _exit(EXIT_FAILURE);
            }
        }
        //step: 6
        //step: 6.1
        if(debugMode){
            fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");
        }
        close(STDIN_FILENO);
        //step: 6.2
        dup(pipefd[0]);
        //step: 6.3
        close(pipefd[0]);
        //step: 6.4
        char* const* arg = cmd2->arguments;
        if(debugMode){
            fprintf(stderr, "(child2>going to execute parsedCmdLines: %s)\n", arg[0]);
        }
        execvp(arg[0], arg);
        perror("execvp");
        _exit(EXIT_FAILURE);
    }
    if(debugMode){
        fprintf(stderr, "(parent_process>created process with id: %d)\n", pid2);
    }
    //step: 7
    if(debugMode){ 
        fprintf(stderr, "(parent_process>closing the read end of the pipe...)\n");
    }
    close(pipefd[0]);
   //step: 8
    if(debugMode) fprintf(stderr, "(parent_process>waiting for child processes to terminate...)\n");
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    if(debugMode) fprintf(stderr, "(parent_process>exiting...)\n");
    addProcess(&myProcessList, cmd1, pid1); 
}

void execute(cmdLine *pCmdLine){
    if(pCmdLine->next){
        process_pipe(pCmdLine, pCmdLine->next);
    } else {
        int pid = fork();
        if(pid == 0){ 
            if(debugMode){
                fprintf(stdout,"PID: %d\n", getpid());
                fprintf(stdout,"Command: %s\n", pCmdLine->arguments[0]);
            }

            if(pCmdLine->inputRedirect){
                fclose(stdin);
                if(fopen(pCmdLine->inputRedirect, "r") == NULL){
                    perror("fopen");
                    _exit(EXIT_FAILURE);
                }
            }
            if(pCmdLine->outputRedirect){
                fclose(stdout);
                if(fopen(pCmdLine->outputRedirect, "w") == NULL){
                    perror("fopen");
                    _exit(EXIT_FAILURE);
                }
            }
            execvp(pCmdLine->arguments[0], pCmdLine->arguments);
            perror("execvp");
            _exit(EXIT_FAILURE);
        }
        if(pCmdLine->blocking){
            waitpid(pid, NULL, 0);
        }
        addProcess(&myProcessList, pCmdLine, pid);
    }
}


char history[HISTLEN][BUFFER];
int newCommand = -1;
int oldCommand = 0;
int historySize;

void addCommandToHistory(char* userInput){
    
    newCommand = (newCommand + 1) % HISTLEN;
    strcpy(history[newCommand], userInput);
    
    if(historySize == HISTLEN){
        oldCommand = (oldCommand + 1) % HISTLEN;
    } else {
        historySize++;
    }
}




void processCommands(cmdLine* pCmdLine, char* userInput){
    if(strcmp(pCmdLine->arguments[0], "history") == 0){
        for(int i = 0; i < historySize; i++){
            printf("%d) %s", i + 1, history[(oldCommand + i) % HISTLEN]);
        }
        freeCmdLines(pCmdLine);
        return;
    } 
    
    else if(strcmp(pCmdLine->arguments[0], "!!") == 0){
        freeCmdLines(pCmdLine);
        if(historySize == 0){
            fprintf(stderr, "History is empty!\n");
            return;
        }
        pCmdLine = parseCmdLines(history[newCommand]);
    } else if(strncmp(pCmdLine->arguments[0], "!", 1) == 0){
        int index = atoi(pCmdLine->arguments[0]+1) - 1;
        freeCmdLines(pCmdLine);
        if(index < 0 || index >= historySize){
            fprintf(stderr, "Index %d out of bounds %d!\n", index, historySize);
            return;
        }
        pCmdLine = parseCmdLines(history[(oldCommand + index) % HISTLEN]);
    } else {
        addCommandToHistory(userInput);
    }
    if(strcmp(pCmdLine->arguments[0], "quit") == 0){
        freeCmdLines(pCmdLine);
        while(myProcessList){
            process* next = myProcessList->next;
            freeProcessList(myProcessList);
            myProcessList = next;
        }
        exit(EXIT_SUCCESS);
    }
    
     if(strcmp(pCmdLine->arguments[0], "kill") == 0){
        if(kill(atoi(pCmdLine->arguments[1]), SIGINT) < 0){
            perror("kill");
        }
        freeCmdLines(pCmdLine);
    }
    else if(strcmp(pCmdLine->arguments[0], "wake") == 0){
        if(kill(atoi(pCmdLine->arguments[1]), SIGCONT) < 0){
            perror("kill");
        }
        freeCmdLines(pCmdLine);
    }

    else if(strcmp(pCmdLine->arguments[0], "suspend") == 0){
        if(kill(atoi(pCmdLine->arguments[1]), SIGTSTP) < 0){
            perror("kill");
        }
        freeCmdLines(pCmdLine);
    }
    
    else if(strcmp(pCmdLine->arguments[0], "procs") == 0){
        printProcessList(&myProcessList);
        freeCmdLines(pCmdLine);
    }
    else {
        execute(pCmdLine);
    }
}


int main(int argc, char** argv){
    char userInput[BUFFER];
    char cwd[BUFFER];
    getcwd(cwd, PATH_MAX);


    for (int i = 0; i < argc; i++){
        if (strcmp (argv[i], "-d") == 0){
            debugMode = 1;
        } else if(debugMode){
            fprintf (stderr, "%s\n", argv[i]);
        }
    }

    while(1){
        printf("%s$ ", cwd);
        if(fgets(userInput, BUFFER, stdin) == NULL){
            while(myProcessList){
                process* next = myProcessList->next;
                freeProcessList(myProcessList);
                myProcessList = next;
            }
            printf("\n");
            break;
        }
        cmdLine* parsedCmdLines = parseCmdLines(userInput);
        
        if(strcmp(parsedCmdLines->arguments[0], "cd") == 0){
            if(chdir(parsedCmdLines->arguments[1]) < 0){
                fprintf (stderr, "%s\n", "chdir");
            } else {
                getcwd(cwd, PATH_MAX);
            }
        }
         else {
            processCommands(parsedCmdLines, userInput);
        }
    }   
}