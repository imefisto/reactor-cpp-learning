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
    // Capture shared_from_this() to keep the handler alive during async operation
    // This prevents use-after-free if the connection is closed before the task completes
    auto self = shared_from_this();
    int fd = fd_; // Capture fd by value

    reactor_->submitTask(
            [message]() {
            return "Async " + message;
            },
            [self, fd] (std::string response) {
                // Verify the connection is still valid before sending
                // The fd might be closed if the client disconnected
                send(fd, response.c_str(), response.length(), 0);
            }
            );
};
