// producer.c
// Build: gcc producer.c -pthread -lrt -o producer
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define SHM_NAME "/pc_shm_table_demo"
#define SEM_EMPTY_NAME "/pc_sem_empty_demo"
#define SEM_FULL_NAME  "/pc_sem_full_demo"
#define SEM_MUTEX_NAME "/pc_sem_mutex_demo"

#define TABLE_CAP 2

typedef struct {
    int buffer[TABLE_CAP];
    int in;          // write index
    int out;         // read index
    int count;       // #items currently on table
    int next_id;     // global item id source
} shared_table_t;

static volatile sig_atomic_t g_run = 1;

static void handle_sigint(int sig){
    (void)sig;
    g_run = 0;
}

typedef struct {
    shared_table_t *tbl;
    sem_t *sem_empty;
    sem_t *sem_full;
    sem_t *sem_mutex;
    int thread_id;
    useconds_t min_us;
    useconds_t max_us;
} producer_args_t;

static useconds_t rand_range(useconds_t a, useconds_t b) {
    if (b <= a) return a;
    return a + (rand() % (b - a + 1));
}

static void* producer_thread(void* arg){
    producer_args_t *pargs = (producer_args_t*)arg;
    while (g_run) {
        // wait for empty slot
        if (sem_wait(pargs->sem_empty) == -1) {
            if (errno == EINTR) continue;
            perror("sem_wait(empty)");
            break;
        }

        // enter critical section
        if (sem_wait(pargs->sem_mutex) == -1) {
            if (errno == EINTR) { sem_post(pargs->sem_empty); continue; }
            perror("sem_wait(mutex)");
            sem_post(pargs->sem_empty);
            break;
        }

        // produce item
        int item_id = pargs->tbl->next_id++;
        pargs->tbl->buffer[pargs->tbl->in] = item_id;
        pargs->tbl->in = (pargs->tbl->in + 1) % TABLE_CAP;
        pargs->tbl->count++;

        printf("[producer #%d] produced item %d | count=%d\n",
               pargs->thread_id, item_id, pargs->tbl->count);
        fflush(stdout);

        // leave critical 
        if (sem_post(pargs->sem_mutex) == -1) {
            perror("sem_post(mutex)");
            break;
        }

        // signal one full slot
        if (sem_post(pargs->sem_full) == -1) {
            perror("sem_post(full)");
            break;
        }

        // random small delay to make the interleaving visible
        usleep(rand_range(pargs->min_us, pargs->max_us));
    }
    return NULL;
}

int main(int argc, char** argv){
    // Usage: ./producer [num_threads] [min_us] [max_us]
    int num_threads = (argc > 1) ? atoi(argv[1]) : 2;
    if (num_threads <= 0) num_threads = 2;
    useconds_t min_us = (argc > 2) ? (useconds_t)strtoul(argv[2], NULL, 10) : 20000;
    useconds_t max_us = (argc > 3) ? (useconds_t)strtoul(argv[3], NULL, 10) : 80000;

    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    struct sigaction sa; memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Create & size shared memory
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) { perror("shm_open"); return 1; }
    if (ftruncate(shm_fd, sizeof(shared_table_t)) == -1) { perror("ftruncate"); return 1; }

    shared_table_t *tbl = mmap(NULL, sizeof(shared_table_t),
                               PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (tbl == MAP_FAILED) { perror("mmap"); return 1; }

    // initialize the shared table 
    bool fresh = (tbl->in == 0 && tbl->out == 0 && tbl->count == 0 && tbl->next_id == 0);
    if (fresh) {
        memset(tbl, 0, sizeof(*tbl));
        printf("[producer] initialized shared table\n");
    }

    // create/open named semaphores
    sem_t *sem_empty = sem_open(SEM_EMPTY_NAME, O_CREAT, 0666, TABLE_CAP);
    if (sem_empty == SEM_FAILED) { perror("sem_open empty"); return 1; }
    sem_t *sem_full  = sem_open(SEM_FULL_NAME,  O_CREAT, 0666, 0);
    if (sem_full  == SEM_FAILED){ perror("sem_open full");  return 1; }
    sem_t *sem_mutex = sem_open(SEM_MUTEX_NAME, O_CREAT, 0666, 1);
    if (sem_mutex == SEM_FAILED){ perror("sem_open mutex"); return 1; }

    printf("[producer] running with %d thread(s). Press Ctrl+C to stop.\n", num_threads);

    // launch producer threads
    pthread_t *threads = calloc((size_t)num_threads, sizeof(pthread_t));
    producer_args_t pargs = {
        .tbl = tbl,
        .sem_empty = sem_empty,
        .sem_full = sem_full,
        .sem_mutex = sem_mutex,
        .thread_id = 0,
        .min_us = min_us,
        .max_us = max_us
    };

    for (int i = 0; i < num_threads; ++i) {
        producer_args_t *arg = malloc(sizeof(producer_args_t));
        *arg = pargs;
        arg->thread_id = i;
        if (pthread_create(&threads[i], NULL, producer_thread, arg) != 0) {
            perror("pthread_create");
            free(arg);
        }
    }

    // wait for signal and join threads
    while (g_run) pause();
    printf("\n[producer] stopping\n");
    for (int i = 0; i < num_threads; ++i) {
        pthread_cancel(threads[i]); 
        pthread_join(threads[i], NULL);
    }
    free(threads);

    sem_close(sem_empty);
    sem_close(sem_full);
    sem_close(sem_mutex);
    sem_unlink(SEM_EMPTY_NAME);
    sem_unlink(SEM_FULL_NAME);
    sem_unlink(SEM_MUTEX_NAME);
    munmap(tbl, sizeof(*tbl));
    close(shm_fd);
    shm_unlink(SHM_NAME);

    printf("[producer] cleaned up\n");
    return 0;
}
