# Understanding the Reactor Pattern with select()

This document explains the Reactor pattern implementation in this codebase, designed as a learning reference.

## What is the Reactor Pattern?

The Reactor pattern is a design pattern for handling service requests delivered concurrently to an application by one or more clients. It's an event-driven pattern that:

1. **Demultiplexes** events from multiple sources (sockets)
2. **Dispatches** them to appropriate handlers
3. Does this all in a **single thread** (no threading complexity!)

Think of it like a restaurant host: customers (events) arrive at different times, and the host (reactor) directs them to the right table (handler).

## Why Use Reactor Pattern?

**Problem it solves**: How do you handle multiple clients efficiently without blocking or creating threads for each connection?

**Traditional approaches**:
- Blocking I/O: Handle one client at a time (slow!)
- Thread-per-connection: Create a thread for each client (resource intensive, context switching overhead)

**Reactor approach**: Use **I/O multiplexing** (select/poll/epoll) to monitor many sockets in a single thread, only processing those that are ready.

## Understanding select()

`select()` is a system call that monitors multiple file descriptors and tells you which ones are ready for I/O.

### How select() Works

```c
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout);
```

**Parameters**:
- `nfds`: Highest file descriptor number + 1 (optimization for the kernel)
- `readfds`: Set of FDs to monitor for reading
- `writefds`: Set of FDs to monitor for writing (we don't use this)
- `exceptfds`: Set of FDs to monitor for exceptions (we don't use this)
- `timeout`: How long to wait (nullptr = block until something happens)

**What it does**:
1. You give it a set of file descriptors to watch
2. It **blocks** until at least one is ready for I/O
3. It **modifies** the sets to indicate which FDs are ready
4. You check each FD to see if it's ready using `FD_ISSET()`

**Key limitation**: In this implementation, select() can handle up to 1024 file descriptors (FD_SETSIZE). For more, you'd use epoll (your next step!).

## Code Walkthrough

### 1. The EventHandler Interface (include/EventHandler.hpp)

```cpp
class EventHandler {
    virtual void handleRead() {}
    virtual void handleWrite() {}
    virtual int getHandle() const = 0;
};
```

**Purpose**: Abstract interface for anything that can handle I/O events.

**Key insight**: By using polymorphism, the Reactor doesn't need to know if it's dealing with a listening socket or a client connection - it just calls `handleRead()`.

### 2. The Reactor (src/Reactor.cpp)

**The heart of the pattern.** Let's break down each method:

#### registerHandler()
```cpp
void Reactor::registerHandler(EventHandlerPtr handler) {
    handlers_[handler->getHandle()] = handler;
}
```
Adds a handler to the map. The key is the file descriptor number.

#### eventLoop() - The Main Loop
```cpp
void Reactor::eventLoop() {
    while (true) {
        // 1. Prepare the fd_set
        fd_set readset;
        FD_ZERO(&readset);  // Clear the set
        int maxfd = 0;

        for (auto &p : handlers_) {
            int fd = p.first;
            FD_SET(fd, &readset);  // Add each FD to the set
            maxfd = std::max(maxfd, fd);
        }

        // 2. Block until something is ready
        int ready = select(maxfd + 1, &readset, nullptr, nullptr, nullptr);

        // 3. Handle ready events
        handleEvents(readset);
    }
}
```

**Step by step**:
1. **Build fd_set**: Add all registered file descriptors to the set
2. **Call select()**: This BLOCKS until at least one FD is ready
3. **Dispatch events**: Call handlers for ready FDs

**Important**: `fd_set` is modified by select(), so we rebuild it every iteration.

#### handleEvents() - Event Dispatch
```cpp
void Reactor::handleEvents(fd_set& readset) {
    // 1. First, collect all ready FDs
    std::vector<int> readyFds;
    for (auto& p : handlers_) {
        if (FD_ISSET(p.first, &readset)) {
            readyFds.push_back(p.first);
        }
    }

    // 2. Then handle them
    for (int fd : readyFds) {
        if (handlers_.count(fd)) {  // Check still exists
            handlers_[fd]->handleRead();
        }
    }
}
```

**Why two loops?** This is subtle but important:
- A handler might call `removeHandler()` during `handleRead()`
- If we iterated `handlers_` directly while calling `handleRead()`, we'd invalidate the iterator
- By copying FDs to a vector first, we avoid this issue

### 3. AcceptorHandler (src/AcceptorHandler.cpp)

**Purpose**: Handles the listening socket - accepts new connections.

```cpp
void AcceptorHandler::handleRead() {
    // 1. Accept new connection
    int client = accept(fd_, (sockaddr*)&addr, &len);

    // 2. Make it non-blocking
    makeNonBlocking(client);

    // 3. Create handler for this connection
    auto h = std::make_shared<ConnectionHandler>(client, reactor_);

    // 4. Register with reactor
    reactor_->registerHandler(h);
}
```

**Key insight**: When the listening socket is "readable", it means a client is trying to connect. Calling `accept()` creates a new socket for that client.

**Why non-blocking?**
- Even though select() says it's ready, `accept()` could still block in edge cases
- Non-blocking ensures we never block the entire event loop

### 4. ConnectionHandler (src/ConnectionHandler.cpp)

**Purpose**: Handles individual client connections - echoes data back.

```cpp
void ConnectionHandler::handleRead() {
    char buffer[1024];
    int bytesRead = recv(fd_, buffer, sizeof(buffer), 0);

    if (bytesRead <= 0) {
        // Client disconnected or error
        reactor_->removeHandler(fd_);
        return;
    }

    // Echo back
    send(fd_, buffer, bytesRead, 0);
}
```

**Flow**:
1. Read data from socket
2. If connection closed (bytesRead <= 0), remove ourselves from reactor
3. Otherwise, echo data back

**Important**: We remove ourselves from the reactor. This is safe because `handleEvents()` copied the FD list.

### 5. Main Function (src/main.cpp)

The initialization sequence:

```cpp
// 1. Create listening socket
int listenFd = socket(AF_INET, SOCK_STREAM, 0);

// 2. Set SO_REUSEADDR (allows quick restart)
setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

// 3. Bind to port 9000
bind(listenFd, (sockaddr*)&addr, sizeof(addr));

// 4. Start listening (backlog = 128)
listen(listenFd, 128);

// 5. Make non-blocking
fcntl(listenFd, F_SETFL, flags | O_NONBLOCK);

// 6. Create acceptor and register
auto acceptor = std::make_shared<AcceptorHandler>(listenFd, &reactor);
reactor.registerHandler(acceptor);

// 7. Run forever
reactor.eventLoop();
```

## Complete Flow Example

Let's trace what happens when a client connects and sends "Hello":

```
1. select() blocks in eventLoop()
2. Client connects → listening socket becomes readable
3. select() returns
4. handleEvents() finds listenFd is ready
5. Calls acceptor->handleRead()
   ├─ accept() creates new socket (fd=4)
   ├─ Creates ConnectionHandler for fd=4
   └─ Registers ConnectionHandler with reactor
6. Back to eventLoop(), select() blocks again
7. Client sends "Hello" → fd=4 becomes readable
8. select() returns
9. handleEvents() finds fd=4 is ready
10. Calls connectionHandler->handleRead()
    ├─ recv() reads "Hello"
    └─ send() echoes "Hello" back
11. Back to eventLoop(), select() blocks again
```

## Key Concepts

### 1. Non-Blocking Sockets

```cpp
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

**Why?** Even though select() says a socket is ready:
- `accept()` might still block in rare race conditions
- `recv()` might block if data arrives then disappears
- Non-blocking returns immediately with EAGAIN/EWOULDBLOCK instead

### 2. File Descriptors are Just Integers

In Unix/Linux, everything is a file:
- Regular files
- Network sockets
- Pipes
- Device files

A file descriptor is just an integer that refers to an open file. The Reactor pattern works because sockets are file descriptors!

### 3. Single-Threaded But Not Blocking

The Reactor is single-threaded, but it's not blocking other clients:
- `select()` blocks, but wakes up when ANY socket is ready
- Once awake, it handles all ready sockets quickly
- Then goes back to select()

**Limitation**: If one handler takes a long time, it blocks others. Keep handlers fast!

### 4. Memory Management

```cpp
using EventHandlerPtr = std::shared_ptr<EventHandler>;
```

- Reactor owns handlers via shared_ptr
- Handlers hold raw pointer to reactor (not owned)
- When handler is removed from map, shared_ptr deletes it automatically

## Common Pitfalls and Solutions

### Pitfall 1: Modifying fd_set During Iteration
**Problem**: `select()` modifies the fd_set, so you can't reuse it.
**Solution**: Rebuild fd_set every iteration (lines 18-26 in Reactor.cpp).

### Pitfall 2: Removing Handler During Dispatch
**Problem**: If `handleRead()` removes itself, you invalidate the iterator.
**Solution**: Copy FDs to vector before calling handlers (lines 40-53 in Reactor.cpp).

### Pitfall 3: Forgetting to Close Socket
**Problem**: File descriptor leak.
**Solution**: In real code, ConnectionHandler should call `close(fd_)` in destructor or when removing.

### Pitfall 4: Assuming recv() Gets All Data
**Problem**: TCP is a stream protocol - `recv()` might only get partial message.
**Solution**: For real protocols, you need buffering and message framing. This echo server is simple enough to not need it.

## Comparing to Swoole

If you're familiar with Swoole:

| Swoole | This Implementation |
|--------|-------------------|
| `Swoole\Server` | `Reactor` |
| `onConnect` callback | `AcceptorHandler::handleRead()` |
| `onReceive` callback | `ConnectionHandler::handleRead()` |
| `$server->send()` | `send()` system call |
| Uses epoll (Linux) | Uses select() |
| Multi-process/thread | Single-threaded |

**Next step (epoll)**: Swoole uses epoll, which is:
- Much faster for many connections (O(1) vs O(n))
- Linux-specific (select is POSIX)
- No 1024 FD limit
- Edge-triggered mode available

## Testing Your Understanding

Try these exercises:

1. **Trace the code**: Use a debugger or add print statements to follow a connection from accept to echo to close.

2. **Test with multiple clients**:
   ```bash
   nc localhost 9000  # Terminal 1
   nc localhost 9000  # Terminal 2
   ```
   Type in each and see them handled independently.

3. **Find the limitation**: What happens if you connect 1025 clients? (Hint: FD_SETSIZE)

4. **Add a feature**: Modify ConnectionHandler to count total bytes received and print on disconnect.

5. **Break it**: Remove the non-blocking calls. What happens? Why?

## Further Reading

- `man 2 select` - The select() man page
- `man 2 accept` - Understanding accept()
- `man 7 tcp` - TCP protocol details
- "Unix Network Programming" by Stevens - The classic book
- Swoole internals - See how they implement Reactor with epoll

## Next Steps: Moving to epoll

Key differences you'll see:
- `epoll_create()` instead of fd_set
- `epoll_ctl()` to add/remove FDs (don't rebuild every time!)
- `epoll_wait()` instead of select() (returns ready FDs directly)
- Edge-triggered vs level-triggered modes
- Much better scalability (C10K problem solution)

Good luck with your learning journey! 🚀
