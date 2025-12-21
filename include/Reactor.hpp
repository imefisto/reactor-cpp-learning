#ifndef REACTOR_H
#define REACTOR_H

#include <chrono>
#include <map>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include "EventHandler.hpp"
#include "Task.hpp"
#include "Timer.hpp"
#include "WorkerPool.hpp"

using HandlerMap = std::unordered_map<int, EventHandlerPtr>;
using TimerMap = std::map<uint64_t, std::vector<Timer>>;

class Reactor {
    public:
        Reactor();
        void registerHandler(EventHandlerPtr handler);
        void removeHandler(int handle);
        void eventLoop();
        int addTimer(uint64_t ms, bool recurring, std::function<void()> cb);
        template<typename TaskFn, typename Continuation>
            void submitTask(TaskFn&& taskFn, Continuation&& continuation)
            {
                Task task;
                task.fn = [taskFn, continuation, this]() {
                    auto result = taskFn();
                    completed_.push([result, continuation]() {
                            continuation(result);
                            });

                    // wake up reactor
                    uint64_t one = 1;
                    write(eventFd_, &one, sizeof(one));
                };
                workerPool_.submit(task);
            }
    private:
        int epollFd_;
        int eventFd_;
        HandlerMap handlers_;
        TimerMap timers_;
        int nextTimerId_ = 0;
        void registerEpollEvent(int fd);
        int computeNextTimerTimeout();
        void processCompletedTasks();
        void processTimers();
        uint64_t nowMs();
        WorkerPool workerPool_;
        std::queue<std::function<void()>> completed_;
        std::mutex completedMtx_;
};

#endif
