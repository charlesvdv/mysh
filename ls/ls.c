#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#define BUF_SIZE 1024

#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0)

int c = -1;
int fd,fc,nread;
char buf[BUF_SIZE];
char sbuf[BUF_SIZE];
struct linux_dirent *d;
struct linux_dirent *sd;
int bpos;
char d_type;
char errno;
int rval;

struct linux_dirent {
    unsigned long  d_ino;     /* Inode number */
    unsigned long  d_off;     /* Offset to next linux_dirent */
    unsigned short d_reclen;  /* Length of this linux_dirent */
    char           d_name[];
}

struct option longopts []{
    {"help", no_argument, 0, 'h'},
    {"directories", no_argument, 0 'd'},
    {"all", no_argument, 0, 'a' },
    {0,0,0,0}
};

int main (int argc, char *argv[]) {

    int h = 0;
    int d = 0;
    int a = 0;

    while ((c = getopt_long(argc, argv, " ", longopts, NULL)) != -1)){

        switch (c){

            case 'h':
                h = 1;
                break;

            case 'd':
                d = 1;
                break;

            case 'a':
                a = 1;
                break;

            case '?':
                fprintf(stderr, "%s: option '-%c' is invalid.\n", argv[0], optopt);
                return 1;

            default:
                return 1;
        }
    }

    fd = open(".", O_RDONLY | O_DIRECTORY);

    if (fd == -1)
        handle_error("open");

    while (1) {
        nread = syscall(SYS_getdents, fd, buf, BUF_SIZE);

        if (nread == -1)
            handle_error("getdents");

        if (nread == 0)
            break;

        if (h == 1){
            printf("ls help \n");
            printf("---------------------");
            printf("-d --directories : includes directories");
            printf("-a --all : shows hidden files");
            printf("---------------------");
        }

        else {
            for (bpos = 0; bpos < nread;) {
                d = (struct linux_dirent *)(buf + bpos);
                d_type = *(buf + bpos + d->d_reclen - 1);

                if(a!=1 & w !=1 & d_type == DT_REG){
                    printf("%s\n", d->d_name);
                }

                if(w == 1 & d_type == DT_DIR){
                    printf("%s\n", d->d_name);
                }

                if(a == 1 & d_type != DT_REG & d_type != DT_DIR){
                    printf("%s\n", d->d_name);
                }
            }
        }
    }
    close(fd);
    exit(EXIT_SUCCESS);
}
