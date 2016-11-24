#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 10

int bg_pid = 0;

char** get_cmd_array(int fd) {
    // Get str from the file descriptor.
    char *str = calloc(BUFFER_SIZE, sizeof(char));
    size_t len = 0;
    int c = 0;
    while (read(fd, &c, 1) == 1) {
        if (c == '\n') { break; }
        if ((++len % BUFFER_SIZE) == 0) {
            str = realloc(str, len + BUFFER_SIZE);
        }
        str[len-1] = c;
    } if (len == 0) {
        // EOF reached.
        free(str);
        return NULL;
    }
    str = realloc(str, len+1);
    str[len] = '\0';

    // Generate array by splitting the str by spaces.
    char **array = NULL;
    char *word = strtok(str, " ");
    int word_num = 0;
    while(word) {
        array = realloc(array, sizeof(char*) * ++word_num);
        array[word_num-1] = (char*) calloc(strlen(word)+1, sizeof(char));
        array[word_num-1] = strncpy(array[word_num-1], word, strlen(word)+1);
        word = strtok(NULL, " ");
    }
    // Add a NULL char at the end of the array for exec().
    array = realloc(array, sizeof(char*) * (word_num + 1));
    array[word_num] = 0;

    free(str);
    return array;
}

void free_cmd(char **array) {
    for (size_t i = 0; array[i] != NULL; i++) {
        free(array[i]);
    }
    free(array);
}

int detect_bg_job(char **cmd) {
    int index = 0;
    int found = 0;
    for ( ; cmd[index] != NULL; index++) {
        if (strncmp(cmd[index], "&", 2) == 0 ) {
            found = 1;
        }
    }

    // Remove the & from the cmd array.
    if (found) {
        for (int i = index - 1; cmd[i] != NULL; i++) {
            cmd[i] = cmd[i+1];
        }
    }
    return found;
}

int launch_shell(int fd, int int_mode) {
    while (1) {
        int bg_mode =  0;

        if (int_mode == 1) {
            write(fd, "$ ", 2);
        }

        // Get command.
        char **cmd = get_cmd_array(fd);
        if (cmd == NULL) {
            if (! int_mode) {
                // We reached an EOF.
                return EXIT_SUCCESS;
            }
            continue;
        }

        // Implement exit.
        size_t cmd_size = 0;
        for ( ; cmd[cmd_size] != NULL; cmd_size++);
        if (strcmp(cmd[0], "exit") == 0) {
            if (cmd_size == 2) {
                int status = atoi(cmd[1]);
                free_cmd(cmd);
                return status;
            }
            free_cmd(cmd);
            if (cmd_size == 1) {
                return EXIT_SUCCESS;
            } else {
                // Error.
                printf("mysh: unknow option for exit");
                return EXIT_FAILURE;
            }
        }

        // Check if the background job is done.
        waitpid(bg_pid, NULL, WNOHANG);
        if (bg_pid != 0 && kill(bg_pid, 0) == -1) {
            bg_pid = 0;
        }

        // Handle foreground cmd.
        if (cmd_size == 1 && strcmp(cmd[0], "fg") == 0) {
            if (bg_pid == 0) {
                printf("No background process.\n");
            } else {
                printf("Process %i is now in foreground.\n", bg_pid);
                waitpid(bg_pid, NULL, 0);
            }
            continue;
        }

        // Handle background job.
        if (detect_bg_job(cmd)) {
            if (bg_pid == 0) {
                bg_mode = 1;
            } else {
                fprintf(stderr, "%s will be running in foreground because %i is " \
                        "already running in background.\n", cmd[0], bg_pid);
            }
        }

        // Execute command.
        pid_t pid = fork();
        if (pid == 0) {
            // In the child.
            int status = EXIT_SUCCESS;
            if (execvp(cmd[0], cmd) == -1) {
                fprintf(stderr, "mysh: Unable to find %s.\n", cmd[0]);
                status = EXIT_FAILURE;
            }
            free_cmd(cmd);
            return status;
        } else if (pid == -1) {
            // Error.
            free_cmd(cmd);
            return EXIT_FAILURE;
        } else {
            // In the parent.
            int flag = 0;
            pid_t w;
            int wstatus;
            if (bg_mode) {
                bg_pid = pid;
                flag = WNOHANG;
            }
            // Wait until child terminaison.
            w = waitpid(pid, &wstatus, flag);
            if (w == -1) {
                // Error.
                free_cmd(cmd);
                return EXIT_FAILURE;
            }
        }
        free_cmd(cmd);
    }
}

int main(int argc, char* argv[]) {
    int int_mode = 1; // Interactive mode.

    if (argc != 1) {
        // Shell is in batch mode.
        int_mode = 0;
        for (int i = 1; i < argc; i++) {
            int script_fd = open(argv[i], 0);
            int status = launch_shell(script_fd, int_mode);
            close(script_fd);
            if (status == EXIT_FAILURE) {
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }
    int status = launch_shell(STDIN_FILENO, int_mode);
    return status;
}
