#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 10

char** get_cmd_array() {
    // Get str from stdin.
    char *str = calloc(BUFFER_SIZE, sizeof(char));
    size_t len = 0;
    int c;
    while((c = fgetc(stdin)) != '\n') {
        if ((++len % BUFFER_SIZE) == 0) {
            str = realloc(str, len + BUFFER_SIZE);
        }
        str[len-1] = c;
    }
    if ((++len % BUFFER_SIZE) == 0) {
        str = realloc(str, len + 1);
    }
    str[len] = '\0';

    // Generate array.
    char **array = NULL;
    char *word = strtok(str, " ");
    int word_num = 0;
    while(word) {
        array = realloc(array, sizeof(char*) * ++word_num);
        array[word_num-1] = (char*) calloc(strlen(word)+1, sizeof(char));
        array[word_num-1] = strncpy(array[word_num-1], word, strlen(word));
        word = strtok(NULL, " ");
    }
    // Add a NULL char at the end of the array for exec().
    array = realloc(array, sizeof(char*) * (word_num + 1));
    array[word_num] = 0;

    free(str);
    return array;
}

void free_cmd(char **array) {
    for (size_t i = 0; i < (sizeof(array)/sizeof(char*)); i++) {
        free(array[i]);
    }
    free(array);
}

int main(int argc, char* argv[]) {
    while (1) {
        // Get command.
        printf("$ ");
        char **cmd = get_cmd_array();

        // Execute command.
        pid_t pid = fork();
        if (pid == 0) {
            // In the child.
            if (execvp(cmd[0], cmd) == -1) {
                free_cmd(cmd);
                exit(EXIT_FAILURE);
            }
        } else if (pid == -1) {
            // Error.
            free_cmd(cmd);
            exit(EXIT_FAILURE);
        } else {
            // In the parent.
            pid_t w;
            int wstatus;
            // Wait until child terminaison.
            w = waitpid(pid, &wstatus, 0);
            if (w == -1) {
                // Error.
                free_cmd(cmd);
                exit(EXIT_FAILURE);
            }
        }
        free_cmd(cmd);
    }
}
