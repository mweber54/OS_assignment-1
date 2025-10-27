# Program Explanation - Producer-Consumer (POSIX Semaphores, Shared Memory, and Threads)

## 1. Overview 

This project implements the **Producer–Consumer problem** using:
- **POSIX named semaphores** for synchronization,
- **POSIX shared memory** for inter-process communication, and
- **POSIX threads** for concurrency within each process.

Two **separate executables** — `producer` and `consumer` — represent independent processes.  
They share a memory segment (`/pc_shm_table_demo`) that acts as a **bounded buffer (table)** capable of holding **2 items** at a time.

When the table is full, producers must **wait**.  
When the table is empty, consumers must **wait**.  
Mutual exclusion is enforced so only one thread (from either process) can access the shared memory at a time.

---

## 2. Shared Memory Structure

Both programs attach to the same shared memory object using:
```c
shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
ftruncate(shm_fd, sizeof(shared_table_t));
mmap(NULL, sizeof(shared_table_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
## Shared Structure
typedef struct {
    int buffer[2];   // bounded buffer (table) with capacity 2
    int in;          // circular write index
    int out;         // circular read index
    int count;       // number of items currently on table
    int next_id;     // unique ID for the next produced item
} shared_table_t;
## Explanation

- The producer writes to `buffer[in]`, increments `in` and `count`, and assigns a new item ID.  
- The consumer reads from `buffer[out]`, increments `out`, and decrements `count`.  
- Both indices wrap around using the modulo operator (`% 2`) to simulate a circular buffer.

---

## 3. Semaphores and Synchronization

Three **named semaphores** coordinate access between the producer and consumer processes:

| Semaphore Name | Initial Value | Purpose |
|----------------|----------------|----------|
| `/pc_sem_empty_demo` | 2 | Counts available empty slots (table capacity = 2) |
| `/pc_sem_full_demo`  | 0 | Counts filled slots; consumers wait when this is 0 |
| `/pc_sem_mutex_demo` | 1 | Binary semaphore enforcing mutual exclusion in critical sections |
## 4. Producer Thread Logic 
``` c
while (g_run) {
    sem_wait(sem_empty);  // wait if no empty slots
    sem_wait(sem_mutex);  // enter critical section

    int item = tbl->next_id++;
    tbl->buffer[tbl->in] = item;
    tbl->in = (tbl->in + 1) % 2;
    tbl->count++;

    printf("[producer #%d] produced item %d | count=%d\n", id, item, tbl->count);

    sem_post(sem_mutex);  // leave critical section
    sem_post(sem_full);   // signal that a new item is available
}
```
- Producers wait on 'sem_empty' when the buffer is full 
- Inside the critical section, they safely add an item 
- Then they signal consumers via 'sem_full'
## 5. Consumer Thread Logic 
``` c
while (g_run) {
    sem_wait(sem_full);   // wait if no items are available
    sem_wait(sem_mutex);  // enter critical section

    int item = tbl->buffer[tbl->out];
    tbl->out = (tbl->out + 1) % 2;
    tbl->count--;

    printf("[consumer #%d] consumed item %d | count=%d\n", id, item, tbl->count);

    sem_post(sem_mutex);  // leave critical section
    sem_post(sem_empty);  // signal that a slot is now free
}
```
- Consumers wait on 'sem_full' whe there are no items.
- Inside the critical section, they safely remove an item
- Then they signal producers via 'sem_empty'
## 6. Producer-Level Communication (Not Thread-Level)
- 'producer' and 'consumer' are distinct executables not threads of one process. 
- They communicate exclusively through:
  - Named shared memory (/pc_shm_table_demo)
  - Named semaphores (/pc_sem_empty_demo, /pc_sem_full_demo, /pc_sem_mutex_demo)
This satisfies the assignment requirement that synchronization happens at the process level, not within the threads of a single program
## 7. Mutual Exclusion
Critical sections where 'in', 'out', 'count', and 'buffer[]' are modified and protected by the binary semapphore 'sem_mutex'. 
This ensures that: 
  - Only one thread at a time accesses shared memory 
  - Race conditions are prevented 
  - Data consistency is maintained across processes
## 8. Signal Handling and Graceful Exit 
Both programs register a SIGINT (Ctrl + C) handler:
``` c
struct sigaction sa;
sa.sa_handler = handle_sigint;
sigaction(SIGINT, &sa, NULL);
```
When interrupted:
  - The global flag 'g_run' becomes 0.
  - Threads exit their loops.
  - The program closes and unlinks semaphores and shared memory 
This prevents leftover shared objects and ensures a clean shutdown. 
## 9. Expected Output
``` bash
[producer #0] produced item 0 | count=1
[producer #1] produced item 1 | count=2
[consumer #0] consumed item 0 | count=1
[consumer #1] consumed item 1 | count=0
...
```
