#ifndef REACTOR_H
#define REACTOR_H

#include <unordered_map>
#include <sys/socket.h>
#include "EventHandler.hpp"

using HandlerMap = std::unordered_map<int, EventHandlerPtr>;

class Reactor {
    public:
        void registerHandler(EventHandlerPtr handler);
        void removeHandler(int handle);
        void eventLoop();
        void handleEvents(fd_set& readset);
    private:
        HandlerMap handlers_;
};

#endif
