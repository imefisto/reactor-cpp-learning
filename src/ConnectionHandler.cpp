#include "ConnectionHandler.hpp"
#include <iostream>

void ConnectionHandler::handleRead() {
    char buffer[1024];
    int bytesRead = recv(fd_, buffer, sizeof(buffer), 0);
    
    if (bytesRead <= 0) {
        std::cout << "[Conn] Closing " << fd_ << std::endl;
        reactor_->removeHandler(fd_);
        return;
    }

    send(fd_, buffer, bytesRead, 0);
};
