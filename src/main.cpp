#include <iostream>

#include "EventHandler.hpp"
#include "Reactor.hpp"

class MockConnectionHandler : public EventHandler {
    public:
        MockConnectionHandler(int id, std::string name) : id_{id}, name_(std::move(name)) {}

        int getHandle() const override { return id_; }

        void handleRead() override {
            std::cout << "[Handler " << name_ << "] handleRead() called" << std::endl;
        }

    private:
        int id_;
        std::string name_;
};

int main() {
    Reactor reactor;

    auto h1 = std::make_shared<MockConnectionHandler>(1, "A");
    auto h2 = std::make_shared<MockConnectionHandler>(2, "B");

    reactor.registerHandler(h1, true, false);
    reactor.registerHandler(h2, true, false);

    std::cout << "\n--- Simulating Reactor loop ---" << std::endl;
    reactor.handleEvents();
    reactor.handleEvents();

    reactor.removeHandler(1);
    std::cout << "\n--- After removing handler 1 <<<" << std::endl;
    reactor.handleEvents();
};
