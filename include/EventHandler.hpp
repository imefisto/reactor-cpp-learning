#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <memory>

class EventHandler {
    public:
        virtual void handleRead() {}
        virtual void handleWrite() {}
        virtual int getHandle() const = 0;
        virtual ~EventHandler() = default;
};

using EventHandlerPtr = std::shared_ptr<EventHandler>;
#endif
