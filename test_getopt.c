#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MSG "Hello World"
#define MSG_LENGHT 500

#define TEXT "Voici un long texte \n" \
             "Il fait 3 lignes\n" \
             "Voila la 3ième"

void strupper(char* str) {
    for(size_t i = 0; i < strnlen(str, MSG_LENGHT); i++) {
        str[i] = toupper(str[i]);
    }
}

struct option longopts[] = {
    {"help", no_argument, 0, 'h'},
    {"name", required_argument, 0, 'n'},
    {"verbose", no_argument, 0, 'v'},
    {0, 0, 0, 0},
};

int main(int argc, char* argv[]) {
    int do_maj = 0;
    int do_v = 0;
    int c;
    char* name = 0;

    while((c = getopt_long(argc, argv, "Mhn:v", longopts, NULL)) != -1) {
        switch (c) {
            case 'M':
                do_maj = 1; break;
            case 'v':
                do_v = 1; break;
            case 'n':
                name = optarg;
                break;
            case 'h':
                printf("This is the help!\n");
                break;
            case ':': // Missing option argument
                fprintf(stderr, "%s: option '-%c' requires an argument.\n", argv[0], optopt);
                break;
            case '?':
            default:
                fprintf(stderr, "%s: option '-%c' is invalid.\n", argv[0], optopt);
                break;
        }

    }

    char* msg = malloc(MSG_LENGHT);
    if (msg == 0) {
        printf("fail\n");
    }
    strncpy(msg, MSG, MSG_LENGHT);
    if (name != 0) {
        char* old_msg = malloc(MSG_LENGHT);
        strcpy(old_msg, msg);
        snprintf(msg, MSG_LENGHT, "%s %s", old_msg, name);
        free(old_msg);
    } if (do_v) {
        char* old_msg = malloc(MSG_LENGHT);
        strcpy(old_msg, msg);
        snprintf(msg, MSG_LENGHT, "%s\n%s", old_msg, TEXT);
        free(old_msg);
    } if (do_maj) {
        strupper(msg);
    }
    printf("%s\n", msg);
    free(msg);
    return 0;
}
