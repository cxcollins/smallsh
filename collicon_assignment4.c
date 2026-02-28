/*
1. set up prompt DONE
2. set up exit DONE
3. set up cd DONE
    will need to use chdir() DONE
4. set up status DONE
    will need keep track of most recent foreground process DONE
5. set up other commands DONE
    add commands to list
6. set up redirection DONE
7. set up background processes
8. set up signal handling

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

pid_t bg_pids[100];
int bg_count = 0;

int last_status = 0;

char *inputFile = NULL;
char *outputFile = NULL;


void exit_function() {
    for (int i = 0; i < bg_count; i++) {
        kill(bg_pids[i], SIGKILL);
    }

    exit(0);
}

void cd_function(char *path) {
    chdir(path);
}

void run_command(char **args) {
    pid_t spawnid;
    int spawnStatus;
    spawnid = fork();
    
    switch(spawnid) {
        case(-1):
            printf("Error forking\n");
            exit(1);
            break;
        case(0):
            if (inputFile != NULL) {
                int fd_in = open(inputFile, O_RDONLY);
                if (fd_in == -1) {
                    fprintf(stderr, "cannot open %s for input\n", inputFile);
                    exit(1);  // last status to 1 also?
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }

            if (outputFile != NULL) {
                int fd_out = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out == -1) {
                    fprintf(stderr, "cannot open %s for output\n", outputFile);
                    exit(1);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }

            if (execvp(args[0], args) == -1) {
                fprintf(stderr, "%s: command not found\n", args[0]);
                exit(1);
            }
            break;
        default:
            if (waitpid(spawnid, &spawnStatus, 0) == -1) {
                perror("waitpid");
                exit(1);
            }
            if (WIFEXITED(spawnStatus)) {
                last_status = WEXITSTATUS(spawnStatus);
            }
            else {
                last_status = WTERMSIG(spawnStatus);
            }
    }
}

void bg_function(char **args) {
    pid_t spawnid_bg;
    int spawnStatus_bg;
    spawnid_bg = fork();
}

int main() {
    
    while (1) {
        // need some sort of flag for if command was previously run - it looks like it's not printing a new line after

        inputFile = NULL;
        outputFile = NULL;

        char input[2048];
        char *saveptr;

        char *args[512];
        int i = 0;
        
        printf(": ");
        fflush(stdout);
        fgets(input, 2048, stdin);
        input[strcspn(input, "\n")] = 0;

        char *command = strtok_r(input, " ", &saveptr);

        if(command == NULL || strcmp(command, "") == 0) {
            continue;
        }

        else if(command[0] == '#') {
            continue;
        }

        else if(strcmp(command, "exit") == 0) {
            exit_function();
        }

        else if(strcmp(command, "status") == 0) {
            printf("exit status: %d\n", last_status);
        }

        else if(strcmp(command, "cd") == 0) {
            char *directory = strtok_r(NULL, " ", &saveptr);

            if (directory == NULL || strlen(directory) == 0) {
                directory = getenv("HOME");
            }
            cd_function(directory);
        }

        else { // add an if?
            char *token = command;  // This is kind of messy but will work for now

            while (token != NULL) {
                if (strcmp(token, "<") == 0) {
                    token = strtok_r(NULL, " ", &saveptr);
                    inputFile = token;
                } else if (strcmp(token, ">") == 0) {
                    token = strtok_r(NULL, " ", &saveptr);
                    outputFile = token;
                } else {
                    args[i] = token;
                    i++;
                }
                token = strtok_r(NULL, " ", &saveptr);
            }
            args[i] = NULL;

            run_command(args);
        }
        

    }

    return 0;
}
