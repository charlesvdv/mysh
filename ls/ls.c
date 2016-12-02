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

#define PRINT_HEADER printf("%s%20s%5s%5s%10s\n", "Name", "Userid", \
                     "Groupid", "Mode", "Size")

struct linux_dirent {
    unsigned long  d_ino;     /* Inode number */
    unsigned long  d_off;     /* Offset to next linux_dirent */
    unsigned short d_reclen;  /* Length of this linux_dirent */
    char           d_name[];
};

struct file_info {
    char *name;
    int userid;
    int groupid;
    int mode;
    long size;
};

struct option longopts[] = {
    {"help", no_argument, 0, 'h'},
    {"exclude", no_argument, 0, 'e'},
    {"all", no_argument, 0, 'a'},
    {"long", no_argument, 0, 'l'},
    {0,0,0,0}
};

int is_directory(int fd) {
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        exit(EXIT_FAILURE);
    }
    return S_ISDIR(sb.st_mode);
}

struct file_info* get_file_info(int dirfd, struct file_info *info) {
    struct stat buf;
    fstatat(dirfd, info->name, &buf, 0);
    info->size = buf.st_size;
    info->groupid = buf.st_gid;
    info->userid = buf.st_uid;
    info->mode = buf.st_mode;
    return info;
}

struct file_info** get_file_list(int fd) {
    struct file_info **infos = malloc(10 * sizeof(struct file_info *));
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
                infos = realloc(infos, (count + 10) * sizeof(struct file_info *));
            }

            infos[count] = malloc(sizeof(struct file_info));

            int str_size = strlen(d->d_name) + 1;
            if (d_type == DT_DIR) { str_size++; }
            infos[count]->name = malloc(str_size);
            strncpy(infos[count]->name, d->d_name, str_size);

            if (d_type == DT_DIR) {
                infos[count]->name[str_size-2] = '/';
            }
            infos[count]->name[str_size-1] = '\0';
            infos[count] = get_file_info(fd, infos[count]);
            count++;
        }
    }
    infos = realloc(infos, (count+1) * sizeof(struct file_info *));
    infos[count] = NULL;
    return infos;
}

void free_file_list(struct file_info **infos) {
    for (int i = 0; infos[i] != NULL; i++) {
        free(infos[i]->name);
        free(infos[i]);
    }
    free(infos);
}

void display_file_display(char *path, char *exclude[], int all_f, int long_f) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "%s is not a valid directory.\n", path);
        exit(EXIT_FAILURE);
    }

    if (long_f) {
        PRINT_HEADER;
    }

    struct file_info **file_list = get_file_list(fd);
    for (int i = 0; file_list[i] != NULL; i++) {
        // Exclude hidden files or all_f is set
        struct file_info *info = file_list[i];
        if (strncmp(info->name, ".", 1) == 0 && !all_f) {
            continue;
        }
        // Exclude from given formats.
        bool found = false;
        for (int j = 0; exclude[j] != NULL; j++) {
            if (strncmp(info->name, exclude[j], strlen(exclude[j])) == 0) {
                found = true;
                break;
            }
        }

        if (found) { continue; }

        if (long_f) {
            printf("%20s%5i%5i%10i%10lu\n", info->name, info->userid,
                    info->groupid, info->mode, info->size);
        } else {
            printf("%s ", info->name);
        }
    }
    if (! long_f) {
        printf("\n");
    }
    free_file_list(file_list);
}

int main (int argc, char *argv[]) {
    int all_f = 0;
    int long_f = 0;
    char *exclude_format[EXCLUDE_DIR_SIZE] = { 0 };

    int c;
    while ((c = getopt_long(argc, argv, "hae:l", longopts, NULL)) != -1) {
        switch (c) {
            case 'h':
                // Print the help.
                printf("usage: ls [-e|--exclude FORMAT] [-a|--all] -- [FILE]...\n"
                        "  ----- Options -----\n" \
                        "  -e\tExclude file based on the format.\n" \
                        "  -a\tShow hidden files.\n" \
                        "  -l\tDisplay more informations about the files.\n");
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
            case 'l':
                long_f = 1;
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
         display_file_display(".", exclude_format, all_f, long_f);
         return EXIT_SUCCESS;
    }

    for (int i = optind; i < argc; i++) {
        display_file_display(argv[i], exclude_format, all_f, long_f);
    }
    return EXIT_SUCCESS;
}
