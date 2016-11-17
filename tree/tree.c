#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
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
    int discovered; // Used for the DFS algorithm.
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

void set_children(INODE *i) {
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
            child->depth = i->depth + 1;

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
    if (i == NULL) { return; }
    INODE *gremlin = i;
    while(1) {
        if (gremlin == NULL) { return; }
        if (gremlin->discovered == discovered) {
            return;
        }
        if (gremlin->isdir) {
            set_children(gremlin);
            recurse_build_tree(gremlin->children, discovered);
        }
        gremlin = gremlin->next;
    }
    i->discovered++;
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

    recurse_build_tree(TREE, TREE->discovered + 1);
    return 0;
}


void free_inode(INODE *i) {
    if (i->fd != -1) { close(i->fd); }
    free(i->name);
    free(i);
}

void clean_tree(INODE *i, int discovered) {
    INODE *gremlin = i;
    while(1) {
        if (gremlin == NULL) { break; }
        if (gremlin->discovered == discovered) {
            break;
        }

        if (gremlin->isdir) {
            clean_tree(gremlin->children, discovered);
        }

        INODE *old_gremlin = gremlin;
        gremlin = gremlin->next;

        free_inode(old_gremlin);
    }
}

void free_tree() {
    clean_tree(TREE, TREE->discovered + 1);
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

bool is_in_array(char *str, char **array) {
    for (int i = 0; array[i] != NULL; i++) {
        if (strcmp(str, array[i]) == 0) {
            return true; 
        } 
    }
    return false;
}

void print_dirtree(INODE *i, int discovered, char **ignored_dir, int max_depht) {
    // Check if directory is empty.
    if (i == NULL) { return; }

    // Check for cli options.
    if (i->depth + 1 >= max_depht) { return; }

    INODE *gremlin = i;
    while(1) {
        if (gremlin == NULL) { break; }
        if (gremlin->discovered == discovered) {
            break;
        }

        bool found = is_in_array(gremlin->name, ignored_dir);
        if (! found) {
            print_inode(gremlin);
        }

        if (gremlin->isdir && ! found) {
            print_dirtree(gremlin->children, discovered, ignored_dir, max_depht);
        }

        gremlin = gremlin->next;
    }
    i->discovered++;
}

int main (int argc, char *argv[]) {
    int c;
    int depht = INT_MAX;
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

    // Run the command for the current directory.
    if (argc - optind == 0) {
        build_inode_tree(".");
        print_dirtree(TREE, TREE->discovered + 1, ignore_dirs, depht);
        free_tree();
        return EXIT_SUCCESS;
    }

    for (int i = optind; i < argc; i++) {
        build_inode_tree(argv[i]);
        print_dirtree(TREE, TREE->discovered + 1,ignore_dirs, depht);
        free_tree();
        // Linebreak to split differents paths.
        if (i == optind && (i+1) != argc) {
            printf("\n");
        }
    }
    return EXIT_SUCCESS;
}
