#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#define IGNORE_DIRS_SIZE 10
#define BUF_SIZE 1024

struct option longopts[] = {
    {"help", no_argument, 0, 'h'},
    {"ignore", required_argument, 0, 'I'},
    {"level", required_argument, 0, 'L'},
    {0, 0, 0, 0}
};

struct linux_dirent {
    long           d_ino;
    off_t          d_off;
    unsigned short d_reclen;
    char           d_name[];
};

typedef struct inode {
    bool isdir;
    char *name;
    int fd;
    int discovered;
    int depth;
    struct inode *children; // First children.
    struct inode *parent;
    struct inode *next;
} INODE;

INODE *TREE;

int is_directory(int fd) {
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        exit(EXIT_FAILURE);
    }
    return S_ISDIR(sb.st_mode);
}

void set_children(INODE *i, int depth) {
    INODE *child = NULL;
    INODE *last_child;
    bool first = true;

    if (! i->isdir) {
        fprintf(stderr, "%s is not a directory.\n", i->name);
    }

    while (1) {
        char buf[BUF_SIZE];
        struct linux_dirent *d = NULL;
        int nread;
        char d_type;

        nread = syscall(SYS_getdents, i->fd, buf, BUF_SIZE);
        if (nread == -1) {
            // Error.
            exit(EXIT_FAILURE);
        }
        if (nread == 0) {
            if (child != NULL) {
                child->next = NULL;
            } else {
                i->children = NULL;
            }
            break;
        }
        for (int bpos = 0; bpos < nread; bpos += d->d_reclen) {
            d = (struct linux_dirent *) (buf + bpos);
            d_type = *(buf + bpos + d->d_reclen - 1);

            if (strncmp(d->d_name, ".", 2) == 0 || strncmp(d->d_name, "..", 3) == 0) {
                continue;
            }

            child = malloc(sizeof(INODE));
            child->name = malloc(strlen(d->d_name) + 1);
            strncpy(child->name, d->d_name, strlen(d->d_name));
            child->name[strlen(d->d_name)] = '\0';
            child->discovered = 0;
            child->depth = depth;

            child->isdir = (d_type == DT_DIR);
            child->parent = i;

            // Get the fd for the child only if the child is a dir.
            if (child->isdir) {
                child->fd = openat(i->fd, child->name, O_RDONLY);
            } else {
                child->fd = -1;
            }

            if (first) {
                i->children = child;
                first = false;
            } else {
                last_child->next = child;
            }

            last_child = child;
        }
    }
}

void recurse_build_tree(INODE *i, int discovered) {
    i->discovered++;
    INODE *gremlin = i->children;

    // Directory is empty.
    if (gremlin == NULL) { return; }

    while(1) {
        if (gremlin->discovered == discovered) {
            break;
        }
        if (gremlin->isdir) {
            set_children(gremlin, gremlin->depth + 1);
            recurse_build_tree(gremlin, discovered);
        }
        if (gremlin->next == NULL) { break; }
        gremlin = gremlin->next;
    }
}

int build_inode_tree(char *name) {
    // Get the file descriptor.
    int fd = open(name, O_RDONLY);
    // Initialize the first inode.
    TREE = malloc(sizeof(INODE));
    if (! is_directory(fd)) {
        fprintf(stderr, "%s is not a directory\n", name);
        exit(EXIT_FAILURE);
    }

    TREE->name = malloc(strlen(name)+1);
    strncpy(TREE->name, name, strlen(name));
    TREE->name[strlen(name)] = '\0';

    TREE->fd = fd;
    TREE->isdir = true;
    TREE->next = NULL;
    TREE->parent = NULL;
    TREE->children = NULL;
    TREE->discovered = 0;
    TREE->depth = 0;

    set_children(TREE, 0);

    recurse_build_tree(TREE, TREE->discovered + 1);
    return 0;
}


void free_inode(INODE *i) {
    free(i->name);
    free(i);
}

void clean_tree(INODE *i, int discovered) {
    i->discovered++;
    INODE *gremlin = i->children;

    // Directory is empty.
    if (gremlin == NULL) { return; }

    while(1) {
        if (gremlin->discovered == discovered) {
            break;
        }
        if (gremlin->isdir) {
            clean_tree(gremlin, discovered);
        }

        if (gremlin->fd != -1) { close(gremlin->fd); }

        INODE *old_gremlin = gremlin;
        gremlin = gremlin->next;

        free_inode(old_gremlin);

        if (gremlin == NULL) { break; }
    }
}

void free_tree() {
    clean_tree(TREE, TREE->discovered + 1);
    free_inode(TREE);
}

void print_inode(INODE *i) {
    char *str = calloc(100, sizeof(char));
    int offset;
    for (offset = 0; offset < i->depth * 2; offset++) {
        str[offset] = ' ';
    }
    str[offset] = '\0';
    strcat(str, i->name);
    if (i->isdir) {
        offset += strlen(i->name);
        str[offset] = '/';
        str[offset+1] = '\0';
    }
    printf("%s\n", str);
    free(str);
}

void print_dirtree(INODE *i, int discovered, char **ignored_dir, int max_depht) {
    i->discovered++;
    INODE *gremlin = i->children;

    // Directory is empty.
    if (gremlin == NULL) { return; }

    while(1) {
        if (gremlin->discovered == discovered) {
            break;
        }
        print_inode(gremlin);
        if (gremlin->isdir) {
            print_dirtree(gremlin, discovered, ignored_dir, max_depht);
        }

        if (gremlin->next == NULL) { break; }
        gremlin = gremlin->next;
    }
}

int main (int argc, char *argv[]) {
    int c;
    int depht = 0;
    char *ignore_dirs[IGNORE_DIRS_SIZE] = {0};

    while ((c = getopt_long(argc, argv, "hI:L:", longopts, NULL)) != -1) {
        switch (c) {
            case 'h':
                // Help.
                break;
            case 'I':
                // Directory to ignore.
                ;
                int offset = 0;
                for ( ; ignore_dirs[offset] != NULL; offset++);
                if (offset == IGNORE_DIRS_SIZE - 1) {
                    fprintf(stderr, "%s: option '-%c' can't be used more than %i\n",
                            argv[0], optopt, IGNORE_DIRS_SIZE);
                    return EXIT_FAILURE;
                }
                ignore_dirs[offset] = optarg;
                break;
            case 'L':
                // Max directory depht.
                depht = atoi(optarg);
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

    argc -= optind;
    if (argc == 0) {
        // Run the command for the current directory.
        build_inode_tree(".");
        print_dirtree(TREE, TREE->discovered + 1, ignore_dirs, depht);
        free_tree();
        return EXIT_SUCCESS;
    }
    for (int i = 1; i <= argc; i++) {
        build_inode_tree(argv[i]);
        print_dirtree(TREE, TREE->discovered + 1,ignore_dirs, depht);
        free_tree();
    }
    return EXIT_SUCCESS;
}
