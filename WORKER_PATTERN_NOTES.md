# Reactor + Worker Pool Pattern - Implementation Notes

## Architecture Overview

This implementation demonstrates a **hybrid Reactor + Worker Pool pattern**, similar to architectures used in:
- PHP Swoole
- Node.js worker threads
- Nginx + upstream workers
- HAProxy event loop

### Core Design

**Single-threaded Reactor** (I/O) + **Multi-threaded Worker Pool** (CPU tasks)

```
┌─────────────────────────────────────────────────────────────┐
│                      REACTOR THREAD                          │
│  ┌──────────┐    ┌──────────┐    ┌──────────────────────┐  │
│  │  epoll   │───>│ Handlers │───>│ Submit Task          │  │
│  │  events  │    │          │    │ to Worker Pool       │  │
│  └──────────┘    └──────────┘    └──────────────────────┘  │
│       │                                     │                │
│       │                                     v                │
│       │                           ┌─────────────────┐       │
│       │                           │   Task Queue    │       │
│       │                           │   (mutex + cv)  │       │
│       │                           └─────────────────┘       │
│       │                                     │                │
│       │                    ┌────────────────┼────────────┐  │
│       │                    v                v            v  │
│       │            ┌──────────┐    ┌──────────┐  ┌────────┐│
│       │            │ Worker 1 │    │ Worker 2 │  │  ...   ││
│       │            └──────────┘    └──────────┘  └────────┘│
│       │                    │                │            │  │
│       │                    └────────────────┴────────────┘  │
│       │                               │                     │
│       │                               v                     │
│       │                    ┌─────────────────────┐         │
│       │                    │ Completed Queue     │         │
│       │                    │ (mutex protected)   │         │
│       │                    └─────────────────────┘         │
│       │                               │                     │
│       │         ┌─────────────────────┘                     │
│       │         │ (eventfd write)                           │
│       v         v                                           │
│  ┌──────────────────────────────┐                          │
│  │  Process Completed Tasks     │                          │
│  │  (send responses to clients) │                          │
│  └──────────────────────────────┘                          │
└─────────────────────────────────────────────────────────────┘
```

## Key Components

### 1. WorkerPool (`include/WorkerPool.hpp`, `src/WorkerPool.cpp`)

**Purpose:** Manages a pool of worker threads that execute CPU-bound tasks.

**Design:**
- Spawns N threads at construction (currently hardcoded to 2)
- Each thread runs `loop()` which blocks on `TaskQueue::pop()`
- On shutdown, pushes dummy tasks to wake all threads, then joins them

**Key insight:** Uses condition variable for efficient blocking (no busy-waiting).

### 2. TaskQueue (`include/TaskQueue.hpp`, `src/TaskQueue.cpp`)

**Purpose:** Thread-safe queue for distributing tasks to workers.

**Design:**
- `std::queue<Task>` protected by mutex
- `std::condition_variable` for blocking pop
- Workers block until work available

**Pattern:** Classic producer-consumer queue.

### 3. Task (`include/Task.hpp`)

**Purpose:** Wrapper for callable work units.

**Design:**
```cpp
struct Task {
    std::function<void()> fn;
};
```

Simple but flexible - can hold any lambda or function pointer.

### 4. Reactor Integration (`include/Reactor.hpp`, `src/Reactor.cpp`)

**Key additions:**
- `WorkerPool workerPool_` member
- `TaskQueue completed_` for results
- `std::mutex completedMtx_` to protect completed queue
- `int eventFd_` for cross-thread signaling
- `submitTask<TaskFn, Continuation>()` template method

**eventfd pattern:**
```cpp
eventFd_ = eventfd(0, EFD_NONBLOCK);
registerHandler(std::make_shared<EventHandler>(eventFd_, [this] {
    processCompletedTasks();
}));
```

Purpose: Workers write to eventfd to wake reactor thread when task completes.

## Data Flow Example

**Client sends "hello\n":**

1. **Reactor thread** receives data via epoll
   - `ConnectionHandler::handleRead()` accumulates message
   - Detects newline, calls `scheduleTask("hello")`

2. **scheduleTask()** submits to worker pool:
   ```cpp
   reactor_->submitTask(
       [msg] { return "Async " + msg; },  // Work function
       [this] (std::string response) {     // Continuation
           send(fd_, response.c_str(), response.length(), 0);
       }
   );
   ```

3. **Worker thread** (Thread 2 or 3):
   - Pops task from queue (blocks on condition variable)
   - Executes work function: `"Async " + "hello"` → `"Async hello"`
   - Pushes continuation to completed queue
   - Writes to eventfd to wake reactor

4. **Reactor thread** wakes up:
   - epoll detects eventfd readable
   - Calls `processCompletedTasks()`
   - Swaps completed queue atomically
   - Executes all continuations (sends responses)

5. **Client receives:** "Async hello"

## Thread Safety Design

### What's Protected:

✅ **Task Queue** (`TaskQueue`)
- Mutex + condition variable
- Safe for multiple producers (reactor) and consumers (workers)

✅ **Completed Queue** (`Reactor::completed_`)
- Protected by `completedMtx_`
- Atomic swap minimizes lock contention

✅ **Worker Pool Lifecycle**
- Atomic `stop_` flag for shutdown
- Proper thread joining

### What's NOT Shared:

✅ **Handler Map** (`Reactor::handlers_`)
- Only accessed by reactor thread
- No locking needed

✅ **epoll File Descriptor**
- Only reactor thread calls `epoll_wait()` and `epoll_ctl()`
- Single-writer pattern

## Comparison to Other Patterns

### vs. Pure Single-Threaded Reactor (Version 1 of this repo)

**Advantages:**
- Can handle CPU-bound work without blocking I/O
- Better CPU utilization on multi-core systems

**Tradeoffs:**
- More complexity (thread synchronization)
- Higher memory usage (thread stacks)
- Context switching overhead

### vs. Multi-Threaded Reactor (Multiple Event Loops)

**This approach** (single reactor + workers):
- One epoll instance, one I/O thread
- Workers are generic compute threads

**Alternative** (multiple reactors):
- Multiple epoll instances, multiple I/O threads
- Each reactor handles subset of connections
- Example: Nginx, memcached

**Tradeoff:** This approach simpler but all I/O goes through one thread (potential bottleneck).

### vs. PHP Swoole

**Similarities:**
- Single reactor thread for I/O multiplexing
- Worker pool for request handling
- Event-driven architecture

**Differences:**

| Aspect | This Implementation | Swoole |
|--------|---------------------|--------|
| Workers | Threads | Processes (fork) |
| Isolation | Shared memory | Process isolation |
| Task ownership | Reactor keeps connection | Worker can own connection |
| Communication | eventfd + mutex | Pipes + message passing |
| Error handling | Crash affects all | Process crash isolated |
| Resource limits | None | Per-worker memory/CPU limits |

**Key insight:** Swoole uses processes for fault isolation. A worker crash doesn't affect reactor or other workers. This uses threads for lower overhead but less isolation.

## Performance Characteristics

### Strengths:

1. **Non-blocking I/O:** Reactor can handle thousands of connections
2. **Parallel processing:** CPU work distributed across cores
3. **Minimal locking:** Task queue only contention point
4. **eventfd efficiency:** Single syscall to wake reactor

### Bottlenecks:

1. **Single I/O thread:** All network I/O serialized through one thread
2. **Task queue lock:** High task submission rate could cause contention
3. **Unbounded queues:** Memory can grow indefinitely under load

### Scalability:

**Good for:**
- High connection count with bursty CPU work
- I/O dominated workloads with occasional heavy computation

**Not ideal for:**
- Purely CPU-bound (workers underutilized, reactor overhead)
- Very high I/O throughput (single reactor thread bottleneck)

## Learning Insights

### Why eventfd vs. Other IPC?

**Alternatives:**
- Pipe: Works but wastes kernel buffer space
- socketpair: Overkill for simple notification
- Signal: Can't integrate with epoll
- Busy polling: Wastes CPU

**eventfd advantages:**
- Integrates with epoll
- Minimal overhead (just a counter)
- Non-blocking mode available

### Why Template for submitTask?

```cpp
template<typename TaskFn, typename Continuation>
void submitTask(TaskFn&& taskFn, Continuation&& continuation)
```

**Benefits:**
- Type safety: Compiler checks argument/return types
- Zero overhead: No virtual dispatch or type erasure
- Flexible: Any callable with any signature

**Tradeoff:** More complex than simple `std::function<void()>` but better performance.

### Why Continuation Pattern?

```cpp
submitTask(
    [] { return work(); },    // Work function
    [] (auto result) { ... }  // Continuation
);
```

**Purpose:** Separates "what to compute" from "what to do with result".

**Benefit:** Reactor thread handles all I/O (sending responses). Workers never touch sockets.

**Alternative:** Workers could send directly, but that requires thread-safe socket handling.

## Production Considerations

This is a **learning implementation**. For production, consider:

1. **Error handling:**
   - Task timeouts
   - Worker health checks
   - Connection error recovery

2. **Resource limits:**
   - Bounded task queue
   - Max connections per worker
   - Memory limits

3. **Observability:**
   - Queue depth metrics
   - Task latency tracking
   - Worker utilization stats

4. **Graceful degradation:**
   - Backpressure when overloaded
   - Circuit breaker for failing workers
   - Rate limiting

5. **Fault isolation:**
   - Consider process-based workers
   - Watchdog for stuck threads
   - Auto-restart failed workers

## Next Learning Steps

### Possible Enhancements:

1. **Multiple reactors:** Explore multi-reactor pattern (à la Nginx)
2. **Task priorities:** Implement priority queue for urgent tasks
3. **Work stealing:** Let idle workers steal from busy workers' queues
4. **Process isolation:** Fork workers instead of threads
5. **Async I/O in workers:** Workers do their own I/O (using io_uring?)
6. **Dynamic worker scaling:** Spawn/destroy workers based on load

### Related Patterns to Study:

- **Leader/Follower:** Alternative to Reactor pattern
- **Proactor:** Asynchronous I/O completion pattern (io_uring, IOCP)
- **Half-Sync/Half-Async:** Layered synchronous/asynchronous processing
- **Active Object:** Encapsulate async execution per object

## References & Further Reading

- **POSA2** (Pattern-Oriented Software Architecture Vol 2): Reactor, Leader/Follower, Half-Sync/Half-Async
- **C++ Concurrency in Action** (Anthony Williams): Thread pools, lock-free queues
- **The Linux Programming Interface** (Kerrisk): epoll, eventfd, threading
- **Nginx architecture:** Multi-process reactor pattern
- **Swoole documentation:** PHP async framework design

---

**Date:** 2025-12-21
**Version:** Reactor with Worker Pool (Tag: with-workers)
**Status:** Learning implementation - has critical issues but demonstrates pattern well
