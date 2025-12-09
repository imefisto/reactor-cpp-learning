#ifndef REACTOR_H
#define REACTOR_H

#include <unordered_map>
#include <vector>
#include <poll.h>
#include <sys/socket.h>
#include "EventHandler.hpp"

using HandlerMap = std::unordered_map<int, EventHandlerPtr>;
using PollfdList = std::vector<struct pollfd>;

class Reactor {
    public:
        void registerHandler(EventHandlerPtr handler);
        void removeHandler(int handle);
        void eventLoop();
        void handleEvents();
    private:
        HandlerMap handlers_;
        PollfdList pollfds_;
};

#endif
