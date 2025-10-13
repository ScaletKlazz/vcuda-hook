#include "client/client.hpp"

#include <utility>

Client& Client::instance() {
    static Client client;
    return client;
}

Client::Client() {
    device_.registerProcessUsageObserver(
        [this](const Device::ProcessUsage& usage) {
            handleProcessUsageUpdate(usage);
        });
}

void Client::registerProcessUsageHandler(ProcessUsageHandler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_.push_back(std::move(handler));
}

void Client::handleProcessUsageUpdate(const Device::ProcessUsage& usage) {
    std::vector<ProcessUsageHandler> handlers_copy;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_copy = handlers_;
    }

    for (auto& handler : handlers_copy) {
        if (handler) {
            handler(usage);
        }
    }
}

void Client::syncForProcess(int /*process_id*/) {
    // TODO: implement cross-process synchronization logic.
}

