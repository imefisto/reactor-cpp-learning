#ifndef REACTOR_H
#define REACTOR_H

#include <unordered_map>
#include "EventHandler.hpp"

using HandlerMap = std::unordered_map<int, EventHandlerPtr>;

class Reactor {
    public:
        void registerHandler(EventHandlerPtr handler, bool forRead, bool forWrite);
        void removeHandler(int handle);
        void handleEvents();
    private:
        HandlerMap handlers_;
};

#endif
