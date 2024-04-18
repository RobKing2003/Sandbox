// Final: final.c
// Robert King
// Final.c goes over all types of coding things that are possible for me to write to study for my CS360 Final
// 12/06/23 (start date)

/* All Important Includes */
#include <stdio.h> // Standard
#include <stdlib.h> // Standard
#include <unistd.h> // getcwd
#include <string.h> // all mem stuff
#include <pthread.h> // Pthreads (mutex/cond)
#include <dirent.h> // DIR*
#include <sys/wait.h> // waitpid
#include <sys/stat.h> // stats
#include <time.h> // ctime

/* Common Colors (1; means bold) */
#define RESET "\e[0m"
#define YELLOW "\e[1;33m" // BOLD
#define BLUE "\e[1;34m" // BOLD
#define RED "\e[1;31m" // BOLD
#define CYAN "\e[36m"
#define GREEN "\e[1;32m" // BOLD

/* Thread Status */
typedef enum {
    WORKING,
    DIE,
    WAITING,
} WorkCommand;

/* Structs */

// Thread Data for cmds and printing
typedef struct {
    char *file_info;
    WorkCommand cmd;
} Thread_Data;

// Main thread data
typedef struct {
    pthread_t *tid;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int num_thread;
    FILE *file;
    char *path[256];
    int file_count;
    int file_tracker;
    DIR *dir;
    char *directory;
    Thread_Data *data;
} Thread;

// Curent Directory
typedef struct {
    char *cwd;
} Directory;

// User Input
typedef struct {
    char *input;
    char *arg[256];
    int arg_count;
    int exit;
    Thread *thread;
} Input;

/* End Structs */

/* Reset_Thread Prototype */
void Reset_Thread(Input *in);

/* Initialize_Threads Prototype */
void Initialize_Threads(Input *in);

void Reset_Input(Input *in);

void Initialize_Threads(Input *in);

void *worker(void *arg);

void Destroy_Threads(Input *in);

/* Prompt User and Directory Path */
void Prompt_User(Directory *dir) {
    getcwd(dir->cwd, 255);

    if (strncmp(dir->cwd, getenv("HOME"), strlen(getenv("HOME"))) == 0) {
        memmove(dir->cwd, "~", 1);
        memmove(dir->cwd + 1, dir->cwd + strlen(getenv("HOME")), strlen(dir->cwd + strlen(getenv("HOME"))) + 1);
    }
    printf(YELLOW "%s@sandbox" RESET ":" BLUE "%s" RESET "> ", getenv("USER"), dir->cwd);
}

/* Marshaller Thread Working Environment */
void Thread_Info(Input *in) {
    in->thread->directory = (char *)malloc(256);
    printf(GREEN "Input a Directory: " RESET);
    scanf("%255s", in->thread->directory);
    getchar();

    if ((in->thread->dir = opendir(in->thread->directory)) == NULL) {
        perror(in->thread->directory);
    }

    in->thread->file_count = 0;
    struct dirent *dent;
    while ((dent = readdir(in->thread->dir)) != NULL) {
        if (dent->d_type == DT_REG) {
            in->thread->file_count++;
        }
    }
    rewinddir(in->thread->dir);

    in->thread->data = (Thread_Data *)malloc(sizeof(Thread_Data) * in->thread->file_count);
    for (int i = 0; i < in->thread->file_count; i++) {
        in->thread->data[i].cmd = WAITING;
    }
    int index = 0;
    while ((dent = readdir(in->thread->dir)) != NULL) {

        if (dent->d_type == DT_REG) {
            in->thread->path[index] = (char *)malloc(strlen(in->thread->directory) + strlen(dent->d_name) + 2);
            sprintf(in->thread->path[index], "%s/%s", in->thread->directory, dent->d_name);
            in->thread->path[index][strlen(in->thread->path[index])] = '\0';

            in->thread->data[index].cmd = WORKING;
            index++;

            pthread_mutex_lock(&in->thread->lock);
            pthread_cond_signal(&in->thread->cond);
            pthread_mutex_unlock(&in->thread->lock);
            if (in->thread->file_count == in->thread->file_tracker) {
                break;
            }
        }
    }


    for (int i = 0; i < in->thread->file_count; i++) {
        if (in->thread->data[i].cmd == WAITING) {
            printf("%s\n", in->thread->data[i].file_info);
        }
        else {
            i--;
        }
    }

    Reset_Thread(in);
}

void Change_Directory(Input *in) {
    if (in->arg[1] == NULL) {
        chdir(getenv("HOME"));
    }
    else if (strcmp(in->arg[1], "~") == 0) {
        chdir(getenv("HOME"));
    }
    else {
        if (chdir(in->arg[1]) != 0) {
            perror(in->arg[1]);
        }
    }
}

/* Executes All Input Commands */
void Execute_Command(Input *in) {
    /* Exit */
    if (strcmp(in->arg[0], "exit") == 0) {
        in->exit = 1;
        return;
    }

    /* Thread Work */
    if (strcmp(in->arg[0], "thread") == 0) {
        Initialize_Threads(in);
        if (in->thread->num_thread < 1) {
            printf(RED "0 Threads have been Initialized.\n" RESET);
            Initialize_Threads(in);
        }
        else {
            Thread_Info(in);
            Destroy_Threads(in);
        }
        return;
    }

    /* Change Directories */
    if (strcmp(in->arg[0], "cd") == 0) {
        Change_Directory(in);
        return;
    }

    /* Fork / Exec Work */
    pid_t pid = fork();

    if (pid == 0) {
        if (execvp(in->arg[0], in->arg) == -1) {
            perror(in->arg[0]);
            exit(1);
        }
    }
    else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
    else if (pid < 0) {
        perror("fork");
//      Reset_Input(in);
    }
}

/* Parses User Input, does not utilize strtok() */
void Parse_Input(Input *in) {
    int length = 0;
    int i = 0;
    in->arg[i] = (char *)malloc(256);

    while(sscanf(in->input + length, "%255s", in->arg[i]) != EOF) {
        in->arg[i][strlen(in->arg[i])] = '\0';
        length += strlen(in->arg[i]) + 1;
        in->arg_count++;

        if (strncmp(in->arg[i], "$", 1) == 0) {
            in->arg[i] += 1;
            char *original = strdup(in->arg[i]);
            in->arg[i] -= 1;
            free(in->arg[i]);
            in->arg[i] = strdup(getenv(original));
            free(original);
        }

        i++;
        in->arg[i] = (char *)malloc(256);
    }
    free(in->arg[i]);
    in->arg[i] = NULL;
}

/* Resets threads for next input */
void Reset_Thread(Input *in) {
    for (int i = 0; i < in->thread->file_count; i++) {
        free(in->thread->data[i].file_info);

        free(in->thread->path[i]);
    }

    free(in->thread->directory);

    closedir(in->thread->dir);

//  in->thread->file_count = 0;
    in->thread->file_tracker = 0;
}

/* Resets Input for next input */
void Reset_Input(Input *in) {
    int i = 0;
    while (in->arg[i] != NULL) {
        free(in->arg[i]);
        in->arg[i] = NULL;
        i++;
    }

    in->arg_count = 0;
    free(in->input);
    in->input = (char *)malloc(256);
}

/* Scans User Input then calls to parse, then to execute, then to reset */
void Scan_Input(Input *in) {
    if (fgets(in->input, 255, stdin) == NULL) {
        in->exit = 1;
        printf("\n");
        return;
    }
    else {
        in->input[strlen(in->input) - 1] = '\0';
    }

    Parse_Input(in);

    Execute_Command(in);

    Reset_Input(in);
}

/* Allocate Directory Struct */
Directory *Allocate_Directory() {
    Directory *dir = (Directory *)malloc(sizeof(Directory));
    dir->cwd = (char *)malloc(256);
    return dir;
}

/* Allocate Input Struct */
Input *Allocate_Input() {
    Input *in = (Input *)malloc(sizeof(Input));
    in->input = (char *)malloc(256);
    in->arg_count = 0;
    in->exit = 0;

    in->thread = (Thread *)malloc(sizeof(Thread));
    in->thread->num_thread = 0;
    in->thread->file_count = 0;
    in->thread->file_tracker = 0;
    pthread_mutex_init(&(in->thread->lock), NULL);
    pthread_cond_init(&(in->thread->cond), NULL);
    return in;
}

/* Deallocate Directory Struct */
void Dealloc_Dir(Directory *dir) {
    free(dir->cwd);
    free(dir);
}

/* Deallocate Input Struct */
void Dealloc_Input(Input *in) {
    free(in->input);
//  free(in-thread->data);
//  free(in->thread);
    free(in);
}

/* Destroy Existing Threads */
void Destroy_Threads(Input *in) {
    for (int i = 0; i < in->thread->file_count; i++) {
        in->thread->data[i].cmd = DIE;
    }
    pthread_cond_broadcast(&(in->thread->cond));

    for (int i = 0; i < in->thread->num_thread; i++) {
        pthread_join(in->thread->tid[i], NULL);
    }

    pthread_mutex_destroy(&(in->thread->lock));
    pthread_cond_destroy(&(in->thread->cond));
    free(in->thread->tid);
}

/* Exit Shell calls for deallocs and destroy */
void Exit_Shell(Directory *dir, Input *in) {
    Dealloc_Dir(dir);
    Dealloc_Input(in);
}


/* Tpool Worker Environment */
void *worker(void *arg) {
    Input *in = arg;

    pthread_mutex_lock(&(in->thread->lock));

    while (1) {
        pthread_cond_wait(&(in->thread->cond), &(in->thread->lock));
        if (in->thread->data[in->thread->file_tracker].cmd == DIE) {
            break;
        }

        if ((in->thread->file = fopen(in->thread->path[in->thread->file_tracker], "r")) == NULL) {
            perror(in->thread->path[in->thread->file_tracker]);
        }

        int fd = fileno(in->thread->file);

        struct stat buf;
        if (fstat(fd, &buf) != 0) {
            perror("fstat");
        }
        in->thread->data[in->thread->file_tracker].file_info = (char *)malloc(256);

        sprintf(in->thread->data[in->thread->file_tracker].file_info, RED "Path: " CYAN "%-25s" RED "Size: " CYAN "%-5ld" RED "Last Mod: " CYAN "%-10s", in->thread->path[in->thread->file_tracker], buf.st_size, ctime(&buf.st_mtime));

        in->thread->data[in->thread->file_tracker].file_info[strlen(in->thread->data[in->thread->file_tracker].file_info) - 1] = '\0';
        close(fd);
        fclose(in->thread->file);

        in->thread->data[in->thread->file_tracker].cmd = WAITING;
        in->thread->file_tracker++;
    }
    pthread_mutex_unlock(&(in->thread->lock));

    return NULL;
}

/* Initializes Tpool */
void Initialize_Threads(Input *in) {
    printf(GREEN "Input Number of Threads (Max: 32): " RESET);
    scanf("%d", &(in->thread->num_thread));
    getchar(); // get newline char that is left behind in stdin

    in->thread->tid = (pthread_t *)malloc(sizeof(pthread_t) * in->thread->num_thread);

    for (int i = 0; i < in->thread->num_thread; i++) {
        pthread_create(&(in->thread->tid[i]), NULL, worker, in);
    }
}

/* Main */
int main() {
    Directory *dir = Allocate_Directory();
    Input *in = Allocate_Input();

//  Initialize_Threads(in);
    while (1) {
        Prompt_User(dir);
        Scan_Input(in);
        if (in->exit == 1) {
            break;
        }
    }
    Exit_Shell(dir, in);
    return 0;
}

/* ---NOTES--- */

/* DIRENT struct */
// struct dirent {
//    ino_t          d_ino;       /* inode number */
//    off_t          d_off;       /* offset to the next dirent */
//    unsigned short d_reclen;    /* length of this record */
//    unsigned char  d_type;      /* type of file; not supported by all file system types */
//    char           d_name[256]; /* filename */
//};

/* STAT struct */
//  struct stat {
//      dev_t     st_dev;         /* ID of device containing file */
//      ino_t     st_ino;         /* Inode number */
//      mode_t    st_mode;        /* File type and mode */
//      nlink_t   st_nlink;       /* Number of hard links */
//      uid_t     st_uid;         /* User ID of owner */
//      gid_t     st_gid;         /* Group ID of owner */
//      dev_t     st_rdev;        /* Device ID (if special file) */
//      off_t     st_size;        /* Total size, in bytes */
//      blksize_t st_blksize;     /* Block size for filesystem I/O */
//      blkcnt_t  st_blocks;      /* Number of 512B blocks allocated */
//      time_t    st_atime;       /* Time of last access */
//      time_t    st_mtime;       /* Time of last modification */
//      time_t    st_ctime;       /* Time of last status change */
//  };                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            3,110         Top
