#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H

#include "EventHandler.hpp"
#include "Reactor.hpp"
#include <memory>

class ConnectionHandler : public EventHandler,
                          public std::enable_shared_from_this<ConnectionHandler> {
    public:
        ConnectionHandler(int fd, Reactor* reactor)
            : fd_(fd), reactor_(reactor) {}

        int getHandle() const override { return fd_; }

        void handleRead() override;

    private:
        int fd_;
        int totalBytesRead_ = 0;
        Reactor* reactor_;
        void scheduleTask(std::string message);
};

#endif
