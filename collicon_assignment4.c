/*
1. set up prompt DONE
2. set up exit DONE
3. set up cd DONE
    will need to use chdir() DONE
4. set up status DONE
    will need keep track of most recent foreground process DONE
5. set up other commands
    add commands to list
6. set up redirection
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

pid_t bg_pids[100];
int bg_count = 0;

int last_status = 0;


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

int main() {
    
    while (1) {
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
                args[i] = token;
                i++;
                token = strtok_r(NULL, " ", &saveptr);
            }
            args[i] = NULL;

            run_command(args);
        }
        

    }

    return 0;
}
