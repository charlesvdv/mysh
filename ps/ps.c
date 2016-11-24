#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define PROCFS_PATH "/proc"

#define FORMAT_ALL "%5i%20s%12s%15s\n"
#define FORMAT_ALL_HEADER "%5s%20s%12s%15s\n"
#define FORMAT_SHORT "%5i%20s\n"
#define FORMAT_SHORT_HEADER "%5s%20s\n"

#define PRINT_HEADER_ALL printf(FORMAT_ALL_HEADER, "PID", "NAME", "VMSIZE", "STATE")
#define PRINT_HEADER_SHORT printf(FORMAT_SHORT_HEADER, "PID", "NAME");

#define STREQ(X, Y) strcmp(X, Y) == 0

// Getopt_long options.
struct option longopts[] = {
    {"help", no_argument, 0, 'h'},
    {"all", required_argument, 0, 'a'},
    {"pid", required_argument, 0, 'p'},
    {0, 0, 0, 0}
};

// Directory entry.
struct linux_dirent {
    long           d_ino;
    off_t          d_off;
    unsigned short d_reclen;
    char           d_name[];
};

// Define a process status.
struct process {
    int pid;
    char *name;
    char *vmsize;
    char *state;
};

// ********** Helper function **********

char* trim_leading_space(char *str) {
    // Removing leading space.
    while (isblank((unsigned char) *str)) { str++; }

    char *out = malloc(strlen(str) + 1);
    memcpy(out, str, strlen(str) + 1);
    return out;
}

// Get a list of the currrent PIDs.
int* get_pids(int procfs_fd) {
    char buf[BUF_SIZE];
    int arr_size = 0;
    int *pids = malloc(10 * sizeof(int));
    struct linux_dirent *d;

    while (1) {
        int nread = syscall(SYS_getdents, procfs_fd, buf, BUF_SIZE);

        if (nread == 0) { break; }
        if (nread == -1) {
            // Error.
            exit(EXIT_FAILURE);
        }

        for (int bpos = 0; bpos < nread; bpos += d->d_reclen) {
            d = (struct linux_dirent *) (buf + bpos);
            char d_type = *(buf + bpos + d->d_reclen -1);

            // Exclude not dir and dir that have not a PID as name.
            if (d_type != DT_DIR || atoi(d->d_name) == 0) {
                continue;
            }

            if ((++arr_size % 10) == 0) {
                pids = realloc(pids, (arr_size + 10) * sizeof(int));
            }
            pids[arr_size - 1] = atoi(d->d_name);
        }
    }
    // Terminate the array with a 0 value.
    pids = realloc(pids, (arr_size + 1) * sizeof(int));
    pids[arr_size] = 0;

    return pids;
}

// ********** PID status functions **********

// Populate the struct process.
void process_status_line(struct process *proc, char *line) {
    // Cut the line into key/value.
    char *key, *value;
    key = strtok(line, ":");
    value = strtok(NULL, ":");

    // Remove useless whitespace in the value.
    char* new = trim_leading_space(value);
    value = new;

    // Populate the struct.
    if (STREQ("Name", key)) {
        proc->name = value;
    } else if (STREQ("VmSize", key)) {
        proc->vmsize = value;
    } else if (STREQ("State", key)) {
        proc->state = value;
    } else {
        // Free if we don't use the value.
        free(value);
    }
}

// Read the /proc/*PID*/status and parse it.
struct process* read_pid_status(int pid) {
    char path[100];
    snprintf(path, 100, "/proc/%i/status", pid);
    int fd = open(path, O_RDONLY);

    char buf[BUF_SIZE];
    int nread;

    char line[BUF_SIZE];
    int line_size = 0;
    struct process *proc = calloc(1, sizeof(struct process));
    while ((nread = read(fd, buf, BUF_SIZE)) > 0) {
        for (int i = 0; i < nread; i++) {
            // EOF.
            if (buf[i] == '\0' || i == BUF_SIZE) { break; }
            // New line.
            if (buf[i] == '\n') {
                line[line_size] = '\0';
                process_status_line(proc, line);

                line_size = 0;
            } else {
                line[line_size] = buf[i];
                line_size++;
            }
        }
    }
    proc->pid = pid;

    if (proc->vmsize == NULL) {
        proc->vmsize = malloc(3);
        strcpy(proc->vmsize, "-1");
    }

    close(fd);
    return proc;
}

struct process** get_processes_status() {
    // Get PIDs.
    int procfd = open(PROCFS_PATH, O_RDONLY);
    int *pids = get_pids(procfd);
    close(procfd);

    // Populate the array of processus status.
    struct process **procs = malloc(sizeof(struct process) * 10);
    int procs_size = 0;
    for (; pids[procs_size] != 0; procs_size++) {
        if (((procs_size + 1) % 10) == 0) {
            procs = realloc(procs, sizeof(struct process) * (procs_size + 10));
        }
        procs[procs_size] = read_pid_status(pids[procs_size]);
    }
    free(pids);

    // Add a NULL at the end of the array.
    procs = realloc(procs, sizeof(struct process *) * (procs_size + 1));
    procs[procs_size] = NULL;

    return procs;
}

int is_pid_valid(struct process **procs, int pid) {
    for (int i = 0; procs[i] != NULL; i++) {
        if (procs[i]->pid == pid) { return 1; }
    }
    return 0;
}

void free_processes(struct process **procs) {
    for (int i = 0; procs[i] != NULL; i++) {
        free(procs[i]->name);
        if (procs[i]->vmsize != NULL) {
            free(procs[i]->vmsize);
        }
        free(procs[i]->state);
        free(procs[i]);
    }
    free(procs);
}

// ********** Display functions **********

void display_proc_all(struct process *p) {
    printf(FORMAT_ALL, p->pid, p->name, p->vmsize, p->state);
}

void display_proc_short(struct process *p) {
    printf(FORMAT_SHORT, p->pid, p->name);
}

int display_procs(struct process **procs, int all_f, int pid) {
    if (pid != 0 && ! is_pid_valid(procs, pid)) {
        fprintf(stderr, "Unable to find %i PID\n", pid);
        return EXIT_FAILURE;
    }

    if (all_f) {
        PRINT_HEADER_ALL;
    } else {
        PRINT_HEADER_SHORT;
    }

    for (int i = 0; procs[i] != NULL; i++) {
        if (all_f && (pid == 0 || procs[i]->pid == pid)) {
            display_proc_all(procs[i]);
        } else if (! all_f && (pid == 0 || procs[i]->pid == pid)) {
            display_proc_short(procs[i]);
        }
    }

    return EXIT_SUCCESS;
}

// ********** Main **********

int main (int argc, char *argv[]) {
    int all_f = 0;
    int c = 0;
    int pid = 0;

    while ((c = getopt_long(argc, argv, "hap:", longopts, NULL)) != -1) {
        switch (c) {
            case 'h':
                // Help.
                printf("usage: ps [-a|--all] [-p|--pid pid]\n" \
                        "  ----- Options -----\n" \
                        "  -a\tGet all the informations.\n" \
                        "  -p\tGive the information about one process.\n");
                return EXIT_SUCCESS;
            case 'a':
                all_f = 1;
                break;
            case 'p':
                // Max directory depth.
                pid = atoi(optarg);
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

    struct process **procs = get_processes_status();
    int status = display_procs(procs, all_f, pid);
    free_processes(procs);

    return status;
}
