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