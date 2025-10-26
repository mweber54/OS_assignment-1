// consumer.c
// Build: gcc consumer.c -pthread -lrt -o consumer
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
    int in;
    int out;
    int count;
    int next_id;
} shared_table_t;

static volatile sig_atomic_t g_run = 1;
static void handle_sigint(int sig){ (void)sig; g_run = 0; }

typedef struct {
    shared_table_t *tbl;
    sem_t *sem_empty;
    sem_t *sem_full;
    sem_t *sem_mutex;
    int thread_id;
    useconds_t min_us;
    useconds_t max_us;
} consumer_args_t;

static useconds_t rand_range(useconds_t a, useconds_t b) {
    if (b <= a) return a;
    return a + (rand() % (b - a + 1));
}

static void* consumer_thread(void* arg){
    consumer_args_t *cargs = (consumer_args_t*)arg;
    while (g_run) {
        // Wait for item available
        if (sem_wait(cargs->sem_full) == -1) {
            if (errno == EINTR) continue;
            perror("sem_wait(full)");
            break;
        }

        // Enter critical section
        if (sem_wait(cargs->sem_mutex) == -1) {
            if (errno == EINTR) { sem_post(cargs->sem_full); continue; }
            perror("sem_wait(mutex)");
            sem_post(cargs->sem_full);
            break;
        }

        int item = cargs->tbl->buffer[cargs->tbl->out];
        cargs->tbl->out = (cargs->tbl->out + 1) % TABLE_CAP;
        cargs->tbl->count--;

        printf("[consumer #%d] consumed item %d | count=%d\n",
               cargs->thread_id, item, cargs->tbl->count);
        fflush(stdout);

        // Leave critical section
        if (sem_post(cargs->sem_mutex) == -1) {
            perror("sem_post(mutex)");
            break;
        }

        // Signal an empty slot
        if (sem_post(cargs->sem_empty) == -1) {
            perror("sem_post(empty)");
            break;
        }

        // random small delay
        usleep(rand_range(cargs->min_us, cargs->max_us));
    }
    return NULL;
}

int main(int argc, char** argv){
    // Usage: ./consumer [num_threads] [min_us] [max_us]
    int num_threads = (argc > 1) ? atoi(argv[1]) : 2;
    if (num_threads <= 0) num_threads = 2;
    useconds_t min_us = (argc > 2) ? (useconds_t)strtoul(argv[2], NULL, 10) : 20000;
    useconds_t max_us = (argc > 3) ? (useconds_t)strtoul(argv[3], NULL, 10) : 90000;

    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    struct sigaction sa; memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Open existing shared memory (spin until producer creates it)
    int shm_fd = -1;
    for (;;) {
        shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (shm_fd != -1) break;
        if (errno != ENOENT) { perror("shm_open"); return 1; }
        usleep(50000); // wait for producer to set it up
    }

    shared_table_t *tbl = mmap(NULL, sizeof(shared_table_t),
                               PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (tbl == MAP_FAILED) { perror("mmap"); return 1; }

    // Open existing semaphores (spin until available)
    sem_t *sem_empty = NULL, *sem_full = NULL, *sem_mutex = NULL;
    while (!sem_empty || !sem_full || !sem_mutex) {
        if (!sem_empty) { sem_empty = sem_open(SEM_EMPTY_NAME, 0); }
        if (!sem_full)  { sem_full  = sem_open(SEM_FULL_NAME, 0);  }
        if (!sem_mutex) { sem_mutex = sem_open(SEM_MUTEX_NAME, 0); }
        if (sem_empty == SEM_FAILED) sem_empty = NULL;
        if (sem_full  == SEM_FAILED) sem_full  = NULL;
        if (sem_mutex == SEM_FAILED) sem_mutex = NULL;
        if (!sem_empty || !sem_full || !sem_mutex) usleep(50000);
    }

    printf("[consumer] running with %d thread(s). Press Ctrl+C to stop.\n", num_threads);

    pthread_t *threads = calloc((size_t)num_threads, sizeof(pthread_t));
    consumer_args_t cargs = {
        .tbl = tbl,
        .sem_empty = sem_empty,
        .sem_full = sem_full,
        .sem_mutex = sem_mutex,
        .thread_id = 0,
        .min_us = min_us,
        .max_us = max_us
    };

    for (int i = 0; i < num_threads; ++i) {
        consumer_args_t *arg = malloc(sizeof(consumer_args_t));
        *arg = cargs;
        arg->thread_id = i;
        if (pthread_create(&threads[i], NULL, consumer_thread, arg) != 0) {
            perror("pthread_create");
            free(arg);
        }
    }

    while (g_run) pause();
    printf("\n[consumer] stopping...\n");
    for (int i = 0; i < num_threads; ++i) {
        pthread_cancel(threads[i]);
        pthread_join(threads[i], NULL);
    }
    free(threads);

    // Cleanup local handles
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_close(sem_mutex);
    munmap(tbl, sizeof(*tbl));
    close(shm_fd);

    // Try unlink; ok if already removed
    sem_unlink(SEM_EMPTY_NAME);
    sem_unlink(SEM_FULL_NAME);
    sem_unlink(SEM_MUTEX_NAME);
    shm_unlink(SHM_NAME);

    printf("[consumer] cleaned up. Bye.\n");
    return 0;
}
