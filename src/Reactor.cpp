#include <algorithm>
#include <iostream>
#include <vector>
#include "Reactor.hpp"

void Reactor::registerHandler(EventHandlerPtr handler) {
    handlers_[handler->getHandle()] = handler;
    std::cout << "[Reactor] Registered handler with id=" << handler->getHandle() << std::endl;
};

void Reactor::removeHandler(int handle) {
    handlers_.erase(handle);
    std::cout << "[Reactor] Removed handler id=" << handle << std::endl;
};

void Reactor::eventLoop() {
    while (true) {
        fd_set readset;
        FD_ZERO(&readset);

        int maxfd = 0;

        for (auto &p : handlers_) {
            int fd = p.first;
            FD_SET(fd,  &readset);
            maxfd = std::max(maxfd, fd);
        }

        int ready = select(maxfd + 1, &readset, nullptr, nullptr, nullptr);
        if (ready < 0) {
            perror("select");
            continue;
        }

        handleEvents(readset);
    }
};

void Reactor::handleEvents(fd_set& readset) {
    std::vector<int> readyFds;

    for (auto& p : handlers_) {
        int fd = p.first;
        if (FD_ISSET(fd, &readset)) {
            readyFds.push_back(fd);
        }
    }

    for (int fd : readyFds) {
        if (handlers_.count(fd)) {
            handlers_[fd]->handleRead();
        }
    }
};
