#include <iostream>
#include <vector>
#include "Reactor.hpp"

void Reactor::registerHandler(EventHandlerPtr handler, bool forRead, bool forWrite) {
    handlers_[handler->getHandle()] = handler;
    std::cout << "[Reactor] Registered handler with id=" << handler->getHandle() << std::endl;
};

void Reactor::removeHandler(int handle) {
    handlers_.erase(handle);
    std::cout << "[Reactor] Removed handler id=" << handle << std::endl;
};

void Reactor::handleEvents() {
    std::vector<int> mockReadyHandles;
    for (auto& [fd, handler] : handlers_) {
        mockReadyHandles.push_back(fd);
    }

    for (int fd : mockReadyHandles) {
        auto handler = handlers_[fd];
        handler->handleRead();
    }
};
