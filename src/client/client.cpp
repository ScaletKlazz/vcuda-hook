

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <fstream>
#include <cstring>
#include <ctime>

#include "client/client.hpp"
#include "spdlog/spdlog.h"
#include "util/logger.hpp"

namespace {
    struct LoggerInitializer {
        LoggerInitializer() {
            util::Logger::init();
        }
    };

    bool isProcessExists(pid_t pid) {
        if (pid <= 0) {
            return false;
        }
        
        std::string path = "/proc/" + std::to_string(pid);
        std::ifstream proc_dir(path);
        if (proc_dir.good()) {
            // Double-check with kill syscall
            return (kill(pid, 0) == 0 || errno == EPERM);
        }
        
        return false;
    }

    LoggerInitializer g_logger_initializer;
}

Client& Client::getInstance() {
    static Client client;
    return client;
}

Client::Client() {
    create_or_attach_process_metric_data();
}

Client::~Client() {
    if (process_metric_data_ != nullptr) {
        munmap(static_cast<void*>(process_metric_data_), SHM_SIZE);
        process_metric_data_ = nullptr;
    }
}

// create or attach shared memory
void Client::create_or_attach_process_metric_data() {
    if (process_metric_data_ != nullptr){
        return;
    }

    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        fd = shm_open(SHM_NAME, O_CREAT | O_RDWR | O_EXCL, 0666);
        if (fd == -1) {
            perror("shm_open create");
            std::exit(EXIT_FAILURE);
        }
    }

    if (ftruncate(fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        close(fd);
        std::exit(EXIT_FAILURE);
    }

    void* ptr = mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        std::exit(EXIT_FAILURE);
    }

    process_metric_data_ = static_cast<MultiProcessMetricData*>(ptr);
    bool expected = false;
    if (process_metric_data_->initialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        std::memset(process_metric_data_, 0, SHM_SIZE);

        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
        pthread_mutex_init(&process_metric_data_->lock, &attr);
        pthread_mutexattr_destroy(&attr);

        process_metric_data_->initialized.store(true, std::memory_order_release);
    } else {
        while (!process_metric_data_->initialized.load(std::memory_order_acquire)) {
            sched_yield();
        }
    }
}

size_t Client::get_device_process_metric_data(int idx){
    size_t summary = 0;
    if (process_metric_data_ == nullptr) {
        return summary;
    }

    if(int rc = pthread_mutex_lock(&process_metric_data_->lock); rc == EOWNERDEAD){
        pthread_mutex_consistent(&process_metric_data_->lock);
    }

    for (int i = 0; i < MAX_PROCESS_NUM; ++i) {
        auto& entry = process_metric_data_->usage[i];

        if (entry.process_id == 0) {
            continue;
        }

        if (!isProcessExists(entry.process_id)) {
            entry.process_id = 0;
            entry.clearUsage();
            continue;
        }

        summary += entry.getUsage(idx);
    }
    pthread_mutex_unlock(&process_metric_data_->lock);

    return summary;
}
void Client::update_process_metric_data(util::ProcessUsage& usage){
    if (process_metric_data_ == nullptr) {
        spdlog::error("process_metric_data_ is null");
        return;
    }

    if(int rc = pthread_mutex_lock(&process_metric_data_->lock); rc == EOWNERDEAD){
        pthread_mutex_consistent(&process_metric_data_->lock);
    }

    int self_slot = -1;
    int free_slot = -1;

    for (int i = 0; i < MAX_PROCESS_NUM; ++i) {
        auto& entry = process_metric_data_->usage[i];

        if (entry.process_id == 0) {
            if (free_slot == -1) {
                free_slot = i;
            }
            continue;
        }

        if (!isProcessExists(entry.process_id)) {
            entry.process_id = 0;
            entry.clearUsage();
            if (free_slot == -1) {
                free_slot = i;
            }
            continue;
        }

        if (entry.process_id == usage.process_id) {
            self_slot = i;
            entry.devices = usage.devices;
            entry.timestamp = usage.timestamp;
        }
    }

    

    if (self_slot == -1 && free_slot != -1) {
        auto& entry = process_metric_data_->usage[free_slot];
        entry.process_id = usage.process_id;
        entry.devices = usage.devices;
        entry.timestamp = usage.timestamp;
        self_slot = free_slot;
    }

    pthread_mutex_unlock(&process_metric_data_->lock);
}
