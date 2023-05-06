#include <stdlib.h>
#include <wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char** argv){

    int debugMode = 0;

    for (int i = 0; i < argc; i++){
        if (strcmp (argv[i], "-d") == 0){
            debugMode = 1;
        } 
    }

    int pipefd[2];
    //step: 1
    if(pipe(pipefd) < 0){
        perror("pipe");
        return 1;
    }
    //step: 2
   if(debugMode) fprintf(stderr, "(parent_process>forking...)\n");
    int pid1 = fork();
    if(pid1 == 0){
        //step: 3
        //step: 3.1
       if(debugMode) fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe...)\n");
        close(STDOUT_FILENO); //or fclose(stdout);
        //step: 3.2
        dup(pipefd[1]);
        //step: 3.3
        close(pipefd[1]);
        //step: 3.4
        char* arg[] = {"ls", "-l", NULL};
       if(debugMode) fprintf(stderr, "(child1>going to execute cmd: %s)\n", arg[0]);
        execvp(arg[0], arg);
        perror("execvp");
        _exit(EXIT_FAILURE);
    } 
   if(debugMode) fprintf(stderr, "(parent_process>created process with id: %d)\n", pid1);
   if(debugMode) fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");
    //step: 4
    close(pipefd[1]);
    //step: 5
   if(debugMode) fprintf(stderr, "(parent_process>forking...)\n");
    int pid2 = fork();
    if(pid2 == 0){
        //step: 6
        //step: 6.1
    if(debugMode) fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");
        close(STDIN_FILENO); 
        //step: 6.2
        dup(pipefd[0]);
        //step: 6.3
        close(pipefd[0]);
        //step: 6.4
        char* arg[] = {"tail", "-n", "2", NULL};
       if(debugMode) fprintf(stderr, "(child2>going to execute cmd: %s)\n", arg[0]);
        execvp(arg[0], arg);
        perror("execvp");
        _exit(EXIT_FAILURE);
    }
   if(debugMode) fprintf(stderr, "(parent_process>created process with id: %d)\n", pid2);
    //step: 7
   if(debugMode) fprintf(stderr, "(parent_process>closing the read end of the pipe...)\n");
    close(pipefd[0]);
    //step: 8
   if(debugMode) fprintf(stderr, "(parent_process>waiting for child processes to terminate...)\n");
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
   if(debugMode) fprintf(stderr, "(parent_process>exiting...)\n");
}

