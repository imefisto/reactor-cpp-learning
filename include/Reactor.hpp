#ifndef REACTOR_H
#define REACTOR_H

#include <unordered_map>
#include <vector>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "EventHandler.hpp"

using HandlerMap = std::unordered_map<int, EventHandlerPtr>;

class Reactor {
    public:
        Reactor();
        void registerHandler(EventHandlerPtr handler);
        void removeHandler(int handle);
        void eventLoop();
    private:
        int epollFd_;
        HandlerMap handlers_;
};

#endif
