#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>

#define BUF_SIZE 1024
#define EXCLUDE_DIR_SIZE 10

struct linux_dirent {
    unsigned long  d_ino;     /* Inode number */
    unsigned long  d_off;     /* Offset to next linux_dirent */
    unsigned short d_reclen;  /* Length of this linux_dirent */
    char           d_name[];
};

struct option longopts[] = {
    {"help", no_argument, 0, 'h'},
    {"exclude", no_argument, 0, 'e'},
    {"all", no_argument, 0, 'a'},
    {0,0,0,0}
};

char** get_file_list(int fd) {
    char **file_list = malloc(10 * sizeof(char *));
    int count = 0;
    while (1) {
        char buf[BUF_SIZE];
        int nread = syscall(SYS_getdents, fd, buf, BUF_SIZE);

        if (nread == -1) {
            exit(EXIT_FAILURE);
        } if (nread == 0) {
            break;
        }
        struct linux_dirent *d;
        for (int bpos = 0; bpos < nread; bpos += d->d_reclen) {
            d = (struct linux_dirent *) (buf + bpos);
            char d_type = *(buf + bpos + d->d_reclen - 1);

            if (((count) % 10) == 0) {
                file_list = realloc(file_list, (count + 10) * sizeof(char*));
            }

            int str_size = strlen(d->d_name) + 1;
            if (d_type == DT_DIR) { str_size++; }
            file_list[count] = malloc(str_size);
            strcpy(file_list[count], d->d_name);

            if (d_type == DT_DIR) {
                file_list[count][str_size-2] = '/';
            }
            file_list[count][str_size-1] = '\0';
            count++;
        }
    }
    file_list = realloc(file_list, (count+1) * sizeof(char*));
    file_list[count] = NULL;
    return file_list;
}

void free_file_list(char** list) {
    for (int i = 0; list[i] != NULL; i++) {
        free(list[i]);
    }
    free(list);
}

void display_file_display(char *path, char *exclude[], int all_f) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "%s is not a valid directory.\n", path);
        exit(EXIT_FAILURE);
    }

    char **file_list = get_file_list(fd);
    for (int i = 0; file_list[i] != NULL; i++) {
        // Exclude hidden files or all_f is set
        if (strncmp(file_list[i], ".", 1) == 0 && !all_f) {
            continue;
        }
        // Exclude from given formats.
        bool found = false;
        for (int j = 0; exclude[j] != NULL; j++) {
            if (strncmp(file_list[i], exclude[j], strlen(exclude[j])) == 0) {
                found = true;
                break;
            }
        }

        if (found) { continue; }

        printf("%s ", file_list[i]);
    }
    printf("\n");
    free_file_list(file_list);
}

int main (int argc, char *argv[]) {
    int all_f = 0;
    char *exclude_format[EXCLUDE_DIR_SIZE] = { 0 };

    int c;
    while ((c = getopt_long(argc, argv, "hae:", longopts, NULL)) != -1) {
        switch (c){
            case 'h':
                // Print the help.
                printf("usage: ls [-e|--exclude FORMAT] [-a|--all] -- [FILE]...\n"
                        "  ----- Options -----\n" \
                        "  -e\tExclude file based on the format.\n" \
                        "  -a\tShow hidden files.\n");
                return EXIT_SUCCESS;
            case 'e':
                ;
                int offset = 0;
                for ( ; exclude_format[offset] != NULL; offset++);
                if (offset == EXCLUDE_DIR_SIZE - 1) {
                    fprintf(stderr, "%s: option '-%c' can't be used more than %i\n",
                            argv[0], optopt, EXCLUDE_DIR_SIZE);
                    return EXIT_FAILURE;
                }
                exclude_format[offset] = optarg;
                break;
            case 'a':
                all_f = 1;
                break;
            case ':':
                // Missing option argument
                fprintf(stderr, "%s: option '-%c' requires an argument.\n", argv[0], optopt);
                break;
            case '?':
            default:
                fprintf(stderr, "%s: option '-%c' is invalid.\n", argv[0], optopt);
            break;
        }
    }

    if ((argc - optind) == 0) {
         display_file_display(".", exclude_format, all_f);
         return EXIT_SUCCESS;
    }

    for (int i = optind; i < argc; i++) {
        display_file_display(argv[i], exclude_format, all_f);
    }
    return EXIT_SUCCESS;
}
