#include <algorithm>
#include <iostream>
#include <vector>
#include <unistd.h>
#include "Reactor.hpp"

void Reactor::registerHandler(EventHandlerPtr handler) {
    int fd = handler->getHandle();
    handlers_[fd] = handler;

    struct pollfd pfd {};
    pfd.fd = fd;
    pfd.events = POLLIN;

    pollfds_.push_back(pfd);

    std::cout << "[Reactor] Registered fd=" << fd << std::endl;
};

void Reactor::removeHandler(int fd) {
    handlers_.erase(fd);

    // Remove from pollfds_
    for (auto it = pollfds_.begin(); it != pollfds_.end(); ++it) {
        if (it->fd == fd) {
            pollfds_.erase(it);
            break;
        }
    }

    std::cout << "[Reactor] Removed fd=" << fd << std::endl;
    close(fd);
};

void Reactor::eventLoop() {
    while (true) {
        int ready = poll(pollfds_.data(), pollfds_.size(), -1);
        if (ready < 0) {
            perror("poll");
            continue;
        }

        handleEvents();
    }
};

void Reactor::handleEvents() {
    for (auto& pfd : pollfds_) {
        if (pfd.revents & POLLIN) {
            int fd = pfd.fd;
            if (handlers_.count(fd)) {
                handlers_[fd]->handleRead();
            }
        }
    }
};
