# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

**Note**: This codebase demonstrates the Reactor pattern with three different I/O implementations across git tags. See `REACTOR_LEARNING.md` for a complete learning guide comparing all versions.

## Build and Run

```bash
mkdir build
cd build
cmake ..
make
./react1
```

The server listens on port 9000 by default.

## Architecture Overview

This is a **Reactor pattern** implementation for a TCP echo server using `epoll()` for I/O multiplexing. The architecture follows a single-threaded event-driven design.

### Core Components

- **Reactor** (`src/Reactor.cpp`, `include/Reactor.hpp`): Central event demultiplexer
  - Maintains a `HandlerMap` (unordered_map) of file descriptors to handlers
  - Creates an epoll instance (`epollFd_`) in constructor
  - Runs `eventLoop()` which calls `epoll_wait()` to monitor all registered file descriptors
  - Uses edge-triggered mode (EPOLLET) for efficient event notification
  - Dispatches events by calling `handleRead()` on ready handlers
  - Provides `registerHandler()` (with `epoll_ctl ADD`) and `removeHandler()` (with `epoll_ctl DEL`) for handler lifecycle management

- **EventHandler** (`include/EventHandler.hpp`): Abstract base class for all handlers
  - Defines interface: `handleRead()`, `handleWrite()`, `getHandle()`
  - Uses smart pointers (`EventHandlerPtr` = `std::shared_ptr<EventHandler>`)

- **AcceptorHandler** (`src/AcceptorHandler.cpp`, `include/AcceptorHandler.hpp`): Handles new connections
  - Wraps the listening socket file descriptor
  - On `handleRead()`: accepts new connections, sets them non-blocking, creates `ConnectionHandler` instances
  - Holds a pointer to the Reactor to register new connection handlers

- **ConnectionHandler** (`src/ConnectionHandler.cpp`, `include/ConnectionHandler.hpp`): Handles client I/O
  - Wraps a client socket file descriptor
  - On `handleRead()`: reads data, echoes it back via `send()`, removes itself from Reactor on disconnect
  - Holds a pointer to the Reactor for self-deregistration

### Data Flow

1. Main creates listening socket, sets it non-blocking, creates AcceptorHandler, registers with Reactor
2. Reactor's `eventLoop()` continuously calls `epoll_wait()` on the epoll instance
3. When listening socket is readable → AcceptorHandler accepts connection → creates ConnectionHandler → registers with Reactor (via `epoll_ctl ADD`)
4. When client socket is readable → ConnectionHandler reads and echoes data back
5. On client disconnect or error (EPOLLHUP/EPOLLERR) → ConnectionHandler removes itself from Reactor (via `epoll_ctl DEL`)

### Important Implementation Details

- All sockets are set to **non-blocking** mode using `fcntl()` with `O_NONBLOCK`
- The Reactor uses `epoll()` with **edge-triggered** mode (EPOLLET) for I/O multiplexing
- Handler removal during event processing is safe: checks `handlers_.count(fd)` before dispatching
- Handlers hold raw pointers to Reactor (not owned), Reactor owns handlers via shared_ptrs
- The epoll instance is created with `epoll_create1(0)` and stored as `epollFd_`
- Events are monitored with a fixed buffer of MAX_EVENTS (64) per `epoll_wait()` call
- The executable name is `react1` (not `reac1` as documented in README)

## Project Structure

- `include/`: Header files for all classes
- `src/`: Implementation files and main.cpp
- Uses C++20 standard
- CMake-based build system with subdirectory structure
