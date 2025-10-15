#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <mutex>
#include <pthread.h>
#include <atomic>

#include "util/usage.hpp"

#define MAX_PROCESS_NUM 8

#define SHM_NAME "vcuda_usage"
#define SHM_SIZE sizeof(MultiProcessMetricData) * MAX_PROCESS_NUM


class Client {
public:
    struct MultiProcessMetricData { // multi process metric data for each process
        std::atomic<bool> initialized{false};
        pthread_mutex_t lock;
        std::array<util::ProcessUsage, MAX_PROCESS_NUM> usage{};
    } __attribute__((aligned(64)));


    static Client& getInstance();

    void create_or_attach_process_metric_data();

    size_t get_device_process_metric_data(int);
    void update_process_metric_data(util::ProcessUsage&);

private:
    Client();
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    MultiProcessMetricData* process_metric_data_ = nullptr;
};

#endif // CLIENT_HPP
