#ifndef REACTOR_H
#define REACTOR_H

#include <chrono>
#include <map>
#include <unordered_map>
#include <vector>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "EventHandler.hpp"
#include "Timer.hpp"

using HandlerMap = std::unordered_map<int, EventHandlerPtr>;
using TimerMap = std::map<uint64_t, std::vector<Timer>>;

class Reactor {
    public:
        Reactor();
        void registerHandler(EventHandlerPtr handler);
        void removeHandler(int handle);
        void eventLoop();
        int addTimer(uint64_t ms, bool recurring, std::function<void()> cb);
    private:
        int epollFd_;
        HandlerMap handlers_;
        TimerMap timers_;
        int nextTimerId_ = 0;
        int computeNextTimerTimeout();
        void processTimers();
        uint64_t nowMs();
};

#endif
