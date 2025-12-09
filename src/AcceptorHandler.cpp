#include <iostream>
#include <netinet/in.h>
#include <fcntl.h>
#include "AcceptorHandler.hpp"
#include "ConnectionHandler.hpp"

void AcceptorHandler::handleRead() {
    while (true) {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        int client = accept4(fd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK);

        if (client < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            perror("accept");
            return;
        }

        std::cout << "[Acceptor] New client fd=" << client << std::endl;

        makeNonBlocking(client);

        // TODO mejorar con using
        auto h = std::make_shared<ConnectionHandler>(client, reactor_);

        reactor_->registerHandler(h);
    }
};

void AcceptorHandler::makeNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
};
