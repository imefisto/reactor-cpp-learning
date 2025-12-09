#include <algorithm>
#include <iostream>
#include <vector>
#include <unistd.h>
#include "Reactor.hpp"

Reactor::Reactor() {
    epollFd_ = epoll_create1(0);
    if (epollFd_ < 0) {
        perror("epoll_create1");
        exit(1);
    }
};

void Reactor::registerHandler(EventHandlerPtr handler) {
    int fd = handler->getHandle();
    handlers_[fd] = handler;

    struct epoll_event ev {};
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        perror("epoll_ctl ADD");
        exit(1);
    }

    std::cout << "[Reactor] Registered fd=" << fd << std::endl;
};

void Reactor::removeHandler(int fd) {
    handlers_.erase(fd);

    if (epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        perror("epoll_ctl DEL");
        exit(1);
    }

    std::cout << "[Reactor] Removed fd=" << fd << std::endl;
    close(fd);
};

void Reactor::eventLoop() {
    const int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];

    while (true) {
        int n = epoll_wait(epollFd_, events, MAX_EVENTS, -1);
        if (n < 0) {
            perror("epoll_wait");
            continue;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            // fd might be removed
            if (!handlers_.count(fd)) continue;

            if (events[i].events & (EPOLLIN)) {
                handlers_[fd]->handleRead();
            }

            if (events[i].events & (EPOLLOUT)) {
                handlers_[fd]->handleWrite();
            }

            if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                std::cout << "[Reactor] HUP/ERR fd=" << fd << std::endl;
                removeHandler(fd);
            }
        }
    }
};
