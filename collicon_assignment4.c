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

volatile sig_atomic_t foreground_mode = 0;
int is_background = 0;

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
            struct sigaction sa_default = {0};
            sa_default.sa_handler = SIG_DFL;
            sigfillset(&sa_default.sa_mask);
            sa_default.sa_flags = 0;
            sigaction(SIGINT, &sa_default, NULL);

            struct sigaction sa_ignore = {0};
            sa_ignore.sa_handler = SIG_IGN;
            sigfillset(&sa_ignore.sa_mask);
            sa_ignore.sa_flags = 0;
            sigaction(SIGTSTP, &sa_ignore, NULL);

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
                if (last_status == SIGINT) {
                    printf("Terminated by signal %d\n", last_status);
                }
            }
    }
}

void bg_function(char **args) {
    pid_t spawnid_bg;
    // int spawnStatus_bg;
    spawnid_bg = fork();

    switch(spawnid_bg) {
        case(-1):
            printf("Error forking\n");
            exit(1);
            break;

        case(0): {
            struct sigaction sa_ignore = {0};
            sa_ignore.sa_handler = SIG_IGN;
            sigfillset(&sa_ignore.sa_mask);
            sa_ignore.sa_flags = 0;
            sigaction(SIGTSTP, &sa_ignore, NULL);

            int fd_in, fd_out;
            
            if (inputFile == NULL) {
                fd_in = open("/dev/null", O_RDONLY);
            }
            else {
                fd_in = open(inputFile, O_RDONLY);
            }
            
            if (outputFile == NULL) {
                fd_out = open("/dev/null", O_WRONLY);
            }
            else {
                fd_out = open(outputFile, O_WRONLY);
            }
            
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);

            if (execvp(args[0], args) == -1) {
                fprintf(stderr, "%s: command not found\n", args[0]);
                exit(1);
            }
        }
        break;

        default: {
            bg_pids[bg_count++] = spawnid_bg;
            printf("Background pid is %d\n", spawnid_bg);
        }
        break;
    }
}

void poll_background_pids() {
    for (int i = 0; i < bg_count; i++) {
        int status;
        pid_t result = waitpid(bg_pids[i], &status, WNOHANG);
        if (result == -1) {
            perror("waitpid");
            exit(1);
        }
        if (result > 0) {
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            }
            else {
                last_status = WTERMSIG(status);
            }
            if (last_status == 15) {
                printf("Background pid %d is done: terminated by signal 15", bg_pids[i]);
            } else {printf("Background pid %d is done: exit status %d\n", bg_pids[i], last_status);}
            
            for (int j = i; j < bg_count - 1; j++) {
                bg_pids[j] = bg_pids[j + 1];
            }
            bg_count--;
            i--;
        }
    }
}

void handle_sigtstp(int sigNo) {
    if (foreground_mode == 0) {
        char* message = "\nEntering foreground-only mode (& is now ignored)\n: ";
        write(STDOUT_FILENO, message, 52);
        foreground_mode = 1;
    } else {
        char* message = "\nExiting foreground-only mode\n: ";
        write(STDOUT_FILENO, message, 33);
        foreground_mode = 0;
    }
}

int main() {
    struct sigaction sa_ignore = {0};
    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_flags = 0;
    sigaction(SIGINT, &sa_ignore, NULL);

    struct sigaction sa_tstp = {0};
    sa_tstp.sa_handler = handle_sigtstp;
    sigfillset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);
    
    while (1) {
        // need some sort of flag for if command was previously run - it looks like it's not printing a new line after

        inputFile = NULL;
        outputFile = NULL;

        char input[2048];
        char *saveptr;

        char *args[512];
        int i = 0;

        poll_background_pids();
        
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
            is_background = 0;  // Reset background flag for each command

            while (token != NULL) {
                if (strcmp(token, "<") == 0) {
                    token = strtok_r(NULL, " ", &saveptr);
                    inputFile = token;
                } else if (strcmp(token, ">") == 0) {
                    token = strtok_r(NULL, " ", &saveptr);
                    outputFile = token;
                } else if (*token == '&') {
                    // Run background processes
                    if (foreground_mode == 0) {
                        is_background = 1;
                        
                        if (is_background) {
                            args[i] = NULL;
                            bg_function(args);
                            break;
                        }
                    }
                } else {
                    args[i] = token;
                    i++;
                }
                token = strtok_r(NULL, " ", &saveptr);
            }

            args[i] = NULL;

            if (is_background == 0) {
                run_command(args);
            }
        }
        

    }

    return 0;
}
