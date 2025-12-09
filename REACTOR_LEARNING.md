# Reactor Pattern Learning Guide

This repository demonstrates the **Reactor pattern** through three different I/O multiplexing implementations. Each version is tagged for easy exploration.

## Quick Navigation

```bash
git checkout reactor-with-select   # Start here
git checkout reactor-with-poll     # Next evolution
git checkout reactor-with-epoll    # Modern approach
git checkout main                  # Current (epoll)
```

## What is the Reactor Pattern?

A single-threaded event-driven architecture where:
1. A central **Reactor** monitors multiple I/O sources (file descriptors)
2. When I/O is ready, the Reactor **dispatches** to the appropriate handler
3. Handlers process events and return control to the Reactor

**Key benefit**: Handle thousands of connections without threads/processes.

---

## The Three Implementations

### 1. SELECT (reactor-with-select)

**The classic Unix approach** - limited but portable.

```cpp
// In eventLoop():
fd_set readset;
FD_ZERO(&readset);
for (auto &p : handlers_) {
    FD_SET(p.first, &readset);
    maxfd = std::max(maxfd, p.first);
}
select(maxfd + 1, &readset, nullptr, nullptr, nullptr);
```

**How it works:**
- Build an `fd_set` bitmap every loop iteration
- `select()` blocks until any fd is ready
- Scan the bitmap to find which fds are ready

**Limitations:**
- Max 1024 file descriptors (FD_SETSIZE)
- O(n) to rebuild fd_set each time
- O(n) to scan for ready fds

**State tracked:** None! Must rebuild everything each iteration.

---

### 2. POLL (reactor-with-poll)

**An improvement over select** - no fd limit.

```cpp
// Member: std::vector<pollfd> pollfds_;

// In registerHandler():
struct pollfd pfd {};
pfd.fd = fd;
pfd.events = POLLIN;
pollfds_.push_back(pfd);

// In eventLoop():
poll(pollfds_.data(), pollfds_.size(), -1);
```

**How it works:**
- Maintain a `vector<pollfd>` of all fds
- `poll()` updates the `revents` field when ready
- Scan the vector to find ready fds

**Improvements over select:**
- No fd limit (beyond system limits)
- Don't rebuild the entire set each time
- Cleaner API (no bit manipulation)

**Limitations:**
- Still O(n) to scan for ready events
- Must maintain the pollfds vector manually

**State tracked:** The `pollfds_` vector persists between iterations.

---

### 3. EPOLL (reactor-with-epoll)

**The modern Linux approach** - true scalability.

```cpp
// Member: int epollFd_;

// In constructor:
epollFd_ = epoll_create1(0);

// In registerHandler():
struct epoll_event ev {};
ev.data.fd = fd;
ev.events = EPOLLIN | EPOLLET;  // Edge-triggered
epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);

// In eventLoop():
struct epoll_event events[MAX_EVENTS];
int n = epoll_wait(epollFd_, events, MAX_EVENTS, -1);
for (int i = 0; i < n; i++) {
    int fd = events[i].data.fd;
    handlers_[fd]->handleRead();
}
```

**How it works:**
- Create an epoll instance (file descriptor)
- Register interest in fds with `epoll_ctl()`
- `epoll_wait()` returns **only ready fds** in an array

**Improvements over poll:**
- O(1) to add/remove/modify interest
- `epoll_wait()` returns **only ready fds** (not all fds)
- Can store user data in `epoll_event.data` (we store the fd)
- Edge-triggered mode available (EPOLLET)

**State tracked:** The kernel maintains the interest set internally.

---

## Key Architectural Differences

| Aspect | SELECT | POLL | EPOLL |
|--------|--------|------|-------|
| **State** | Stateless (rebuild each loop) | Vector of pollfds | Kernel-managed interest set |
| **Scaling** | O(n) scan, max 1024 fds | O(n) scan, no limit | O(1) operations, O(k) where k=ready |
| **API calls** | `select()` | `poll()` | `epoll_create1()`, `epoll_ctl()`, `epoll_wait()` |
| **Event notification** | Level-triggered only | Level-triggered only | Level or edge-triggered |
| **Portability** | POSIX (everywhere) | POSIX (everywhere) | Linux only |

---

## What Each Version Teaches

### SELECT teaches:
- The fundamental Reactor pattern
- Why we need I/O multiplexing
- The concept of "readiness" vs blocking

### POLL teaches:
- How maintaining state improves performance
- The difference between interest (events) and results (revents)
- Why removing the fd limit matters

### EPOLL teaches:
- How the kernel can help us scale
- The power of event registration
- Edge-triggered vs level-triggered notifications

---

## Common Pattern Across All Versions

Despite different I/O mechanisms, the **architecture stays the same**:

```
┌─────────────────────────────────────────┐
│  main()                                 │
│  - Creates listening socket             │
│  - Creates AcceptorHandler              │
│  - Registers with Reactor               │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  Reactor::eventLoop()                   │
│  - Waits for I/O (select/poll/epoll)   │
│  - Dispatches to handlers               │
└──────────────┬──────────────────────────┘
               │
       ┌───────┴────────┐
       ▼                ▼
┌─────────────┐  ┌─────────────┐
│  Acceptor   │  │ Connection  │
│  Handler    │  │  Handler    │
│  - accept() │  │  - read()   │
│  - creates  │  │  - echo     │
│    new      │  │  - send()   │
│    clients  │  │             │
└─────────────┘  └─────────────┘
```

---

## Testing Your Understanding

After time passes, refresh your memory by:

1. **Rebuild & run**: See the echo server work
   ```bash
   mkdir build && cd build && cmake .. && make && ./react1
   ```

2. **Compare implementations**: Look at `src/Reactor.cpp` across tags
   ```bash
   git diff reactor-with-select reactor-with-poll src/Reactor.cpp
   ```

3. **Trace a connection**: Add debug prints and watch:
   - Listening socket readiness
   - AcceptorHandler creating ConnectionHandler
   - Client data echoed back

4. **Ask yourself**:
   - Why does select need to rebuild fd_set each time?
   - How does poll avoid the 1024 fd limit?
   - Why does epoll scale to 10k+ connections?

---

## Further Learning

- Try breaking it: What happens with 10,000 connections on select vs epoll?
- Implement write buffering: Handle EAGAIN on send()
- Add timer support: Periodic tasks without breaking the event loop
- Compare to other patterns: Thread-per-connection, thread pool, async/await

---

**Remember**: The Reactor pattern is about **inversion of control**. Instead of your code blocking on I/O, the Reactor tells you when I/O is ready. This simple idea scales from 10 to 100,000 connections.
