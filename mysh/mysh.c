#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFFER_SIZE 10

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

int launch_shell(int fd, int int_mode) {
    while (1) {
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

        // Execute command.
        pid_t pid = fork();
        if (pid == 0) {
            // In the child.
            if (execvp(cmd[0], cmd) == -1) {
                free_cmd(cmd);
                return EXIT_FAILURE;
            }
        } else if (pid == -1) {
            // Error.
            free_cmd(cmd);
            return EXIT_FAILURE;
        } else {
            // In the parent.
            pid_t w;
            int wstatus;
            // Wait until child terminaison.
            w = waitpid(pid, &wstatus, 0);
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
