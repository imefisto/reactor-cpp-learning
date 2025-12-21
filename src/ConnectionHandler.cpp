#include "ConnectionHandler.hpp"
#include <iostream>
#include <unistd.h>

void ConnectionHandler::handleRead() {
    std::string message("");
    char buffer[4096];

    while (true) {
        ssize_t n = recv(fd_, buffer, sizeof(buffer), 0);

        if (n > 0) {
            std::string chunk(buffer, n);
            auto pos = chunk.find("\n");
            if (pos != std::string::npos) {
                message.append(chunk);
                scheduleTask(message);
            } else {
                message.append(chunk);
            }
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

void ConnectionHandler::scheduleTask(std::string message)
{
    reactor_->submitTask(
            [message]() {
            return "Async " + message;
            },
            [this] (std::string response) {
                send(fd_, response.c_str(), response.length(), 0); // echo
            }
            );
};
