# Producer–Consumer (POSIX Semaphores + Shared Memory + Threads)

Two **separate processes** (`producer` and `consumer`) coordinate via **POSIX named semaphores** and a **POSIX shared memory** segment that acts as a bounded buffer (“table”) of capacity **2**.  
Each process internally uses **threads** (configurable). Mutual exclusion is enforced with a binary semaphore.

> **How it meets the assignment:** process-level communication via shared memory + named semaphores; proper synchronization (empty/full/mutex); mutual exclusion around the critical section; builds and runs on Linux/WSL.

---

## Build (Linux / WSL)

### Option A — Makefile (recommended)
```bash
make

### Option B - Manual gcc
gcc src/producer.c -pthread -lrt -o producer
gcc src/consumer.c -pthread -lrt -o consumer

## Run
Open one or two terminals 
./producer &   # or: ./producer 2 20000 80000 &
./consumer &   # or: ./consumer 2 30000 90000 &
Arguments (optional)
./producer [num_threads] [min_us] [max_us]
./consumer [num_threads] [min_us] [max_us]
Stop with Ctrl+C

Expected Output (sample)
[producer] initialized shared table
[producer] running with 2 thread(s). Press Ctrl+C to stop.
[consumer] running with 2 thread(s). Press Ctrl+C to stop.

[producer #0] produced item 0 | count=1
[producer #1] produced item 1 | count=2
[consumer #0] consumed item 0 | count=1
[consumer #1] consumed item 1 | count=0
...

Files
producer.c - producer process with N producer threads
consumer.c - consumer process with N consumer threads
Makefile - build rules for Linux/WSL
docs/EXPLANATION.md - detailed design (semaphores, shared memory, mutual exclusion)
examples/ - screenshots of executed results

