#include "ConnectionHandler.hpp"
#include <iostream>
#include <unistd.h>

void ConnectionHandler::handleRead() {
    char buffer[4096];

    while (true) {
        ssize_t n = recv(fd_, buffer, sizeof(buffer), 0);

        if (n > 0) {
            send(fd_, buffer, n, 0); // echo
        } else if (n == 0) {
            std::cout << "[Conn] Closing " << fd_ << std::endl;
            close(fd_);
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data, exit loop
                std::cout << "[Conn] No more data, exit loop " << fd_ << std::endl;
                return;
            } else {
                perror("recv");
                reactor_->removeHandler(fd_);
                return;
            }
        }
    }
};
