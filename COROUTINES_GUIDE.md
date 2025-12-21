# C++20 Coroutines Guide for Reactor Pattern

## Why Coroutines for Your MUD?

Currently, your async code looks like this:
```cpp
reactor_->submitTask(
    [message]() { return "Async " + message; },    // Work function
    [self, fd](std::string response) {              // Continuation (callback)
        send(fd, response.c_str(), response.length(), 0);
    }
);
```

With coroutines, it becomes:
```cpp
Task<void> handleCommand(std::string message) {
    std::string response = co_await asyncWork([message]() {
        return "Async " + message;
    });
    send(fd_, response.c_str(), response.length(), 0);
}
```

**Benefits:**
- Sequential, readable code (no callback hell)
- Exception handling with try/catch
- Local variables persist across suspensions
- Perfect for multi-step game logic (combat rounds, quests)

---

## C++20 Coroutines Crash Course

### The Three Keywords

1. **`co_await`** - Suspend and wait for async operation
2. **`co_return`** - Return value from coroutine
3. **`co_yield`** - Return value and suspend (for generators)

Any function using these keywords becomes a coroutine.

### Coroutine Anatomy

```cpp
Task<int> myCoroutine() {       // Return type must be "awaitable"
    int x = co_await someWork(); // Suspend here until work completes
    co_return x + 1;             // Return value
}
```

When compiled, this transforms into a state machine with:
- **Promise object** - Manages coroutine state
- **Coroutine handle** - Pointer to resume/destroy the coroutine
- **Awaitable object** - Defines suspension points

---

## Implementation Roadmap

### Phase 1: Core Types (45-60 min)

You need to implement three main types:

#### 1. `Task<T>` - The Coroutine Return Type

**File:** `include/CoroTask.hpp`

```cpp
template<typename T>
class Task {
public:
    // Coroutine interface (promise_type is required by compiler)
    struct promise_type {
        // Called when coroutine is created
        Task get_return_object();
        
        // Should coroutine start suspended?
        std::suspend_always initial_suspend() { return {}; }
        
        // What happens at the end?
        auto final_suspend() noexcept;
        
        // Handle return value
        void return_value(T value);
        
        // Handle exceptions
        void unhandled_exception();
        
        // Store result and continuation
        std::optional<T> result;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;
    };

    // Awaitable interface (allows co_await Task)
    bool await_ready() const noexcept;
    auto await_suspend(std::coroutine_handle<> cont) noexcept;
    T await_resume();

    // Lifecycle
    Task(std::coroutine_handle<promise_type> h);
    ~Task();
    Task(Task&&) noexcept;
    Task& operator=(Task&&) noexcept;

private:
    std::coroutine_handle<promise_type> coro_;
};
```

**Key Insights:**
- `promise_type` is required by the C++ compiler for coroutines
- `await_*` methods make Task co_awaitable
- Store continuation handle for resumption chain
- RAII: coroutine handle must be destroyed in destructor

**Specialization needed:**
- `Task<void>` (no result storage, `return_void()` instead of `return_value()`)

#### 2. `AsyncWork<T>` - Bridge to Worker Pool

**File:** `include/AsyncWork.hpp`

```cpp
template<typename T>
class AsyncWork {
public:
    using WorkFn = std::function<T()>;
    
    AsyncWork(WorkFn work) : work_(std::move(work)) {}
    
    // Awaitable interface
    bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> cont) {
        continuation_ = cont;
        // Don't resume yet - Reactor will submit to worker pool
    }
    
    T await_resume() {
        return std::move(*result_);
    }
    
    // Called by worker thread
    void execute() {
        result_ = work_();
    }
    
    // Called by reactor thread after completion
    void resume() {
        continuation_.resume();
    }
    
    WorkFn work_;
    std::coroutine_handle<> continuation_;
    std::optional<T> result_;
};
```

**Flow:**
1. Game code: `co_await AsyncWork{[]{ /* heavy work */ }}`
2. `await_suspend` captures continuation handle
3. Reactor submits work to worker pool
4. Worker calls `execute()`, stores result
5. Worker notifies reactor via eventfd
6. Reactor calls `resume()` → continuation proceeds

#### 3. `Reactor::spawn()` - Start Coroutines

**File:** `include/Reactor.hpp` (add method)

```cpp
template<typename T>
void spawn(Task<T> task) {
    // Start the coroutine (it will suspend at first co_await)
    task.start();
}
```

---

### Phase 2: Integration with Reactor (30-45 min)

#### Modify Reactor to Handle AsyncWork

**Challenge:** When a coroutine does `co_await AsyncWork{work}`:
1. Coroutine suspends
2. We need to submit work to worker pool
3. When done, resume the coroutine

**Solution:** Add overload to `submitTask`:

```cpp
// In Reactor.hpp
template<typename T>
void submitWork(AsyncWork<T>* work) {
    workerPool_.submit([work]() {
        work->execute();  // Run work in worker thread
        
        {
            std::lock_guard<std::mutex> lock(completedMtx_);
            completed_.push([work]() {
                work->resume();  // Resume coroutine in reactor thread
            });
        }
        
        uint64_t one = 1;
        write(eventFd_, &one, sizeof(one));
    });
}
```

**Key:** The coroutine handle is resumed on the reactor thread (single-threaded safety).

#### Create Helper Function

```cpp
// In AsyncWork.hpp
template<typename T, typename WorkFn>
Task<T> asyncWork(WorkFn&& work) {
    AsyncWork<T> aw{std::forward<WorkFn>(work)};
    
    // This will call Reactor::submitWork() somehow
    // Options:
    // 1. Pass reactor pointer to asyncWork
    // 2. Use thread_local reactor pointer
    // 3. Store reactor in GameSession
    
    co_return co_await aw;
}
```

**Design choice:** How does `AsyncWork` find the `Reactor`?
- **Option A:** Thread-local global (simple but less clean)
- **Option B:** Pass reactor explicitly to `asyncWork(reactor, fn)`
- **Option C:** Store in GameSession, pass via context

I recommend Option B for clarity during learning.

---

### Phase 3: GameSession with Coroutines (45-60 min)

Replace `ConnectionHandler` with coroutine-based `GameSession`:

```cpp
class GameSession : public EventHandler,
                    public std::enable_shared_from_this<GameSession> {
public:
    GameSession(int fd, Reactor* reactor, World* world);
    
    void handleRead() override;
    
    // Main game loop (coroutine)
    Task<void> gameLoop();
    
    // Command handlers (all coroutines)
    Task<void> handleLookCommand();
    Task<void> handleGoCommand(std::string direction);
    Task<void> handleAttackCommand(std::string target);
    
private:
    int fd_;
    Reactor* reactor_;
    World* world_;
    Player* player_;
    std::string inputBuffer_;
    
    void send(const std::string& message);
    Task<std::string> readLine();  // Async read from client
};
```

**Example: Async Combat**

```cpp
Task<void> GameSession::handleAttackCommand(std::string targetName) {
    NPC* target = player_->currentRoom()->findNPC(targetName);
    if (!target) {
        send("There's no " + targetName + " here.\n");
        co_return;
    }
    
    send("You attack " + targetName + "!\n");
    
    // Offload combat calculation to worker
    auto [playerDamage, npcDamage] = co_await asyncWork<CombatResult>(reactor_, [=]() {
        return calculateCombat(player_, target);  // Heavy calculation
    });
    
    // Back on reactor thread
    target->takeDamage(npcDamage);
    send("You deal " + std::to_string(npcDamage) + " damage!\n");
    
    if (target->isDead()) {
        send(targetName + " is defeated!\n");
        player_->currentRoom()->removeNPC(target);
        co_return;
    }
    
    // NPC counterattack
    player_->takeDamage(playerDamage);
    send(targetName + " hits you for " + std::to_string(playerDamage) + " damage!\n");
    
    if (player_->isDead()) {
        send("You have been defeated!\n");
        // Respawn logic
    }
}
```

**Notice:** Linear flow despite async operations. No callbacks!

---

## Testing Your Coroutine Implementation

### Test 1: Simple Task

```cpp
Task<int> simpleTest() {
    co_return 42;
}

// In main:
Task<int> t = simpleTest();
t.start();
assert(t.done());
// t.result() should be 42
```

### Test 2: Chained Tasks

```cpp
Task<int> inner() {
    co_return 10;
}

Task<int> outer() {
    int x = co_await inner();
    co_return x * 2;
}

// Result should be 20
```

### Test 3: AsyncWork Integration

```cpp
Task<void> testAsyncWork() {
    int result = co_await asyncWork<int>(reactor, []() {
        return 42;  // Runs in worker thread
    });
    // Now back on reactor thread
    assert(result == 42);
}
```

---

## Common Pitfalls

### ❌ Pitfall 1: Coroutine Not Started

```cpp
Task<void> foo() { co_return; }

void bar() {
    auto t = foo();  // Created but not started!
    // t goes out of scope → coroutine destroyed without running
}
```

**Fix:** Always start tasks: `reactor->spawn(foo());`

### ❌ Pitfall 2: Dangling References

```cpp
Task<void> bad(const std::string& msg) {
    co_await someWork();
    // msg might be destroyed if caller didn't keep it alive!
    send(msg);
}
```

**Fix:** Capture by value or use `shared_ptr`:
```cpp
Task<void> good(std::string msg) {  // Copy
    co_await someWork();
    send(msg);  // Safe
}
```

### ❌ Pitfall 3: Exception in Coroutine

```cpp
Task<void> throws() {
    throw std::runtime_error("oops");
    co_return;  // Never reached
}
```

**Fix:** Implement `unhandled_exception()` in promise:
```cpp
void unhandled_exception() {
    exception_ = std::current_exception();
}
```

Then rethrow in `await_resume()`.

### ❌ Pitfall 4: Thread Safety

```cpp
Task<void> bad() {
    auto result = co_await asyncWork(...);
    handlers_[fd] = ...;  // ❌ handlers_ should only be touched by reactor thread
}
```

**Rule:** Only touch shared state on the reactor thread. Worker threads are for pure computation.

---

## Architecture Summary

```
┌─────────────────────────────────────────────────────────┐
│                    REACTOR THREAD                        │
│                                                          │
│  GameSession::handleAttackCommand() ◄── coroutine       │
│       │                                                  │
│       ├─► co_await asyncWork(calculateCombat)           │
│       │        │                                         │
│       │        └──► AsyncWork::await_suspend()          │
│       │                  │                               │
│       │                  └──► Reactor::submitWork()     │
│       │                            │                     │
│       │                            ▼                     │
│       │                    ┌──────────────┐             │
│       │                    │ Worker Pool  │             │
│       │                    │  execute()   │             │
│       │                    └──────────────┘             │
│       │                            │                     │
│       │                            │ (eventfd notify)    │
│       │                            ▼                     │
│       │                  Reactor::processCompleted()    │
│       │                            │                     │
│       │                            └──► AsyncWork::resume()
│       │                                        │         │
│       ◄────────────────────────────────────────┘        │
│       │                                                  │
│       └─► send("You deal 15 damage!\n")                 │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

**Key Points:**
1. Coroutines always resume on reactor thread (thread-safe)
2. Workers only execute pure functions
3. No shared state between threads (except queues)
4. Coroutine state persists across suspensions

---

## Next Steps

1. **Implement Task<T>** in `include/CoroTask.hpp`
   - Start with `Task<int>` for simplicity
   - Test with simple coroutines
   - Add `Task<void>` specialization
   
2. **Implement AsyncWork<T>** in `include/AsyncWork.hpp`
   - Integrate with `Reactor::submitWork()`
   - Test with worker pool
   
3. **Create GameSession** in `include/GameSession.hpp`
   - Replace ConnectionHandler
   - Implement command parsing
   - Add first coroutine command (e.g., `look`)
   
4. **Build MUD Components** (see MUD_IMPLEMENTATION_GUIDE.md)
   - World, Room, Player, NPC classes
   - Command handlers using coroutines

---

## Resources

- **CppCoro library** (Lewis Baker): Reference implementation of coroutines
- **C++ Reference**: https://en.cppreference.com/w/cpp/language/coroutines
- **Lewiss Baker's blog**: Understanding C++20 Coroutines
- **"C++ Concurrency in Action"** (2nd ed): Chapter 14 on coroutines

---

**Remember:** Coroutines are challenging at first, but once you understand the pattern, they make async code dramatically cleaner. Take it step by step, test incrementally, and you'll have a solid foundation for building complex game logic.

Good luck! 🚀
