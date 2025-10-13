#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <vector>
#include <atomic>
#include <functional>
#include <mutex>
#include <pthread.h>

#include "device/device.hpp"

#define MAX_PROCESS_NUM 8


class Client {
public:
    struct MultiProcessMetricData { // multi process metric data for each process
        std::atomic<bool> initialized{false};
        pthread_mutex_t lock;
        std::vector<Device::ProcessUsage> usage;
    };

    using ProcessUsageHandler = std::function<void(const Device::ProcessUsage&)>;

    static Client& instance();

    void registerProcessUsageHandler(ProcessUsageHandler handler);

    void syncForProcess(int process_id);

private:
    Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    void handleProcessUsageUpdate(const Device::ProcessUsage& usage);

    std::mutex handlers_mutex_;
    std::vector<ProcessUsageHandler> handlers_;
    Device device_;
};

#endif // CLIENT_HPP
