# Critical Issues & Quick Wins

## 🔴 CRITICAL: Race Condition in Completion Queue

**Location:** `include/Reactor.hpp:34` (inside `submitTask` template)

**Problem:**
```cpp
completed_.push([result, continuation]() {
    continuation(result);
});
```

Workers push to `completed_` queue **without holding `completedMtx_`**!

**Impact:** Data race that can cause:
- Queue corruption
- Segmentation faults
- Silent data loss
- Undefined behavior

**Why it happens:** The lambda captures and pushes to `completed_`, but `completedMtx_` is only held during `processCompletedTasks()` when swapping the queue. Multiple workers can push simultaneously.

**Fix needed:**
```cpp
{
    std::lock_guard<std::mutex> lock(completedMtx_);
    completed_.push([result, continuation]() {
        continuation(result);
    });
}
```

---

## 🔴 HIGH PRIORITY: File Descriptor Lifetime Issue

**Location:** `src/ConnectionHandler.cpp:46` (inside `scheduleTask`)

**Problem:**
```cpp
[this] (std::string response) {
    send(fd_, response.c_str(), response.length(), 0);
}
```

**Scenario:**
1. Client sends data, task queued to worker
2. Client disconnects, ConnectionHandler destroyed, `fd_` closed
3. Worker finishes, continuation runs on reactor thread
4. Continuation tries to send on closed/reused file descriptor

**Impact:**
- Send to wrong connection (if fd reused)
- EBADF error
- Potential security issue if fd assigned to different client

**Fix options:**
1. Use `shared_from_this()` to keep handler alive:
   ```cpp
   auto self = shared_from_this();
   [self, message]() { /* work */ }
   ```
2. Validate fd before send (check if still in handlers map)
3. Add connection state tracking (connected/disconnected flag)

---

## 🟡 MEDIUM: Performance - Unnecessary Copies

**Location:** `src/TaskQueue.cpp:26` (in `pop()`)

**Problem:**
```cpp
Task t = queue_.front();
queue_.pop();
return t;  // Copies std::function
```

**Impact:** Copies `std::function` which can be expensive if it captures large objects.

**Fix:**
```cpp
return std::move(t);
```

---

## 🟡 MEDIUM: Missing Error Handling

**Location:** Multiple places where `eventfd` is written

**Problem:**
```cpp
write(eventFd_, &one, sizeof(one));  // No error check
```

**Impact:** Silent failure if eventfd buffer is full (unlikely but possible under extreme load).

**Fix:** Check return value and log errors.

---

## 🟢 LOW: Hardcoded Worker Count

**Location:** `src/Reactor.cpp:9` (constructor)

**Problem:**
```cpp
Reactor::Reactor() : workerPool_(2) { ... }
```

**Issue:** Worker count should scale with available CPU cores.

**Suggestion:** Make configurable via constructor parameter or detect via `std::thread::hardware_concurrency()`.

---

## 🟢 LOW: No Backpressure Mechanism

**Current state:** Unbounded task queue can grow indefinitely under high load.

**Risk:** Memory exhaustion if tasks arrive faster than workers can process.

**Suggestion:**
- Add bounded queue with max size
- Reject tasks when queue full (return error to client)
- Or block accepting new connections when overloaded

---

## Summary Priority

1. **Fix race condition** (critical for correctness)
2. **Fix fd lifetime** (critical for stability)
3. **Add move semantics** (easy performance win)
4. **Add error handling** (robustness)
5. **Make workers configurable** (flexibility)
6. **Add backpressure** (production readiness)

## Testing Recommendations

To expose the race condition:
- Run with ThreadSanitizer: `cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread" ..`
- High concurrency test: multiple clients sending rapidly
- Stress test: `ab` or `wrk` with high connection count

To test fd lifetime issue:
- Send message then immediately close connection
- Should see crashes or wrong-connection sends
