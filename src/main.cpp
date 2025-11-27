#include <iostream>
#include <unordered_map>
#include <memory>
#include <vector>

class EventHandler {
    public:
        virtual void handleRead() {}
        virtual void handleWrite() {}
        virtual int getHandle() const = 0;
        virtual ~EventHandler() = default;
};

using EventHandlerPtr = std::shared_ptr<EventHandler>;
using HandlerMap = std::unordered_map<int, EventHandlerPtr>;

class Reactor {
    public:
        void registerHandler(EventHandlerPtr handler, bool forRead, bool forWrite) {
            handlers_[handler->getHandle()] = handler;
            std::cout << "[Reactor] Registered handler with id=" << handler->getHandle() << std::endl;
        }

        void removeHandler(int handle) {
            handlers_.erase(handle);
            std::cout << "[Reactor] Removed handler id=" << handle << std::endl;
        }

        void handleEvents() {
            std::vector<int> mockReadyHandles;
            for (auto& [fd, handler] : handlers_) {
                mockReadyHandles.push_back(fd);
            }

            for (int fd : mockReadyHandles) {
                auto handler = handlers_[fd];
                handler->handleRead();
            }
        }
    private:
        HandlerMap handlers_;
};

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
