# Producer–Consumer (POSIX Semaphores + Shared Memory + Threads)

Two **separate processes** — `producer` and `consumer` — coordinate through **POSIX named semaphores** and a **POSIX shared memory** segment that acts as a bounded buffer (“table”) of capacity **2**.  
Each process internally uses **threads** (configurable), and **mutual exclusion** is enforced using a binary semaphore
---

## Build (Linux / WSL)

### Option A — Using Makefile (recommended)
``` bash
make
```

### Option B — Manual Compilation
``` bash
gcc producer.c -pthread -lrt -o producer
gcc consumer.c -pthread -lrt -o consumer
```

---

## Run

Open one or two terminals and run:
``` bash
./producer & # or: ./producer 2 20000 80000 &
./consumer & # or: ./consumer 2 30000 90000 &
```


### Arguments (optional)
```bash
./producer [num_threads] [min_us] [max_us]
./consumer [num_threads] [min_us] [max_us]
```

- `num_threads`: number of producer/consumer threads (default = 2)  
- `min_us`, `max_us`: sleep time (microseconds) to visualize concurrency  

Stop with **Ctrl + C** in each terminal.

---

## Semaphores Used

| Semaphore Name | Initial Value | Purpose |
|----------------|----------------|----------|
| `/pc_sem_empty_demo` | 2 | Counts available empty slots (table capacity = 2) |
| `/pc_sem_full_demo`  | 0 | Counts filled slots; consumers wait when 0 |
| `/pc_sem_mutex_demo` | 1 | Binary semaphore ensuring mutual exclusion |

---

## Shared Memory Structure

| Field | Type | Description |
|--------|------|--------------|
| `buffer[2]` | `int` array | The bounded table shared between producer and consumer |
| `in` | `int` | Circular write index used by producers |
| `out` | `int` | Circular read index used by consumers |
| `count` | `int` | Current number of items on the table |
| `next_id` | `int` | Unique ID assigned to each produced item |

Shared memory object name: `/pc_shm_table_demo`

---

## Expected Output (Sample)
[producer] initialized shared table
[producer] running with 2 thread(s). Press Ctrl+C to stop.
[consumer] running with 2 thread(s). Press Ctrl+C to stop.

[producer #0] produced item 0 | count=1
[producer #1] produced item 1 | count=2
[consumer #0] consumed item 0 | count=1
[consumer #1] consumed item 1 | count=0
...

**Interpretation**
- The `count` value oscillates between **0 and 2**, matching the table’s capacity.  
- Producers **wait** when the table is full.  
- Consumers **wait** when the table is empty.  
- Mutual exclusion ensures consistent updates without race conditions.

---

## Files Included

| File | Description |
|------|--------------|
| `producer.c` | Producer process with N producer threads |
| `consumer.c` | Consumer process with N consumer threads |
| `Makefile` | Build rules for Linux/WSL |
| `docs/EXPLANATION.md` | Detailed explanation of semaphores, shared memory, and mutual exclusion |
| `examples/` | Screenshots of executed results |

---
