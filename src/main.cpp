#include <iostream>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <fcntl.h>
#include "AcceptorHandler.hpp"
#include "EventHandler.hpp"
#include "Reactor.hpp"

int main() {
    Reactor reactor;

    int listenFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listenFd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9000);

    if (bind(listenFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(listenFd, 128) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "[Main] Listening on port 9000 ..." << std::endl;

    // int flags = fcntl(listenFd, F_GETFL, 0);
    // fcntl(listenFd, F_SETFL, flags | O_NONBLOCK);

    auto acceptor = std::make_shared<AcceptorHandler>(listenFd, &reactor);
    reactor.registerHandler(acceptor);

    reactor.addTimer(1000, true, []() {
            std::cout << "Timer every 1s" << std::endl;
            });

    reactor.eventLoop();

    return 0;
};
