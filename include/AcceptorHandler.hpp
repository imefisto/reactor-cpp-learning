#ifndef ACCEPTOR_HANDLER_H
#define ACCEPTOR_HANDLER_H

#include "EventHandler.hpp"
#include "Reactor.hpp"

class AcceptorHandler : public EventHandler {
    public:
        AcceptorHandler(int fd, Reactor* reactor)
            : fd_(fd), reactor_(reactor) {}

        int getHandle() const override { return fd_; }

        void handleRead() override;

    private:
        int fd_;
        Reactor* reactor_;
        void makeNonBlocking(int fd);
};

#endif
