#include "device/device.hpp"
#include <utility>
#include "spdlog/spdlog.h"
#include "util/logger.hpp"
#include "util/config.hpp"

namespace {
    struct LoggerInitializer {
        LoggerInitializer() {
            util::Logger::init();
        }
    };

    LoggerInitializer g_logger_initializer;
}

// Device constructor
Device::Device()
{ 
    process_usage_ = ProcessUsage{};
    device_memory_blocks_.resize(DEVICE_MAX_NUM);

    if (auto limit = util::Config::memoryLimitBytes();limit > 0) {
        device_memory_limit_bytes_ = limit;
    }

    if (auto deviceName = util::Config::targetDeviceName();size(deviceName) > 0) {
        device_name_ = deviceName;
    }
}

void Device::setDeviceId(int device_id) {
    spdlog::info("Set device id to {}", device_id);
    device_id_ = device_id;
}

int Device::getDeviceId() {
    return device_id_;
}


// record allocation action
void Device::recordAllocation(CUdeviceptr ptr, size_t size, int idx) {
    std::vector<ProcessUsageListener> listeners;
    ProcessUsage snapshot;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        process_usage_.devices[idx].usage += size;
        process_usage_.timestamp = std::time(nullptr);
        device_memory_blocks_[idx][ptr] = MemoryBlock{ptr, size};

        snapshot = process_usage_;
        listeners = process_usage_listeners_;
    }

    for (auto& listener : listeners) {
        if (listener) {
            listener(snapshot);
        }
    }
}

// record free action
void Device::recordFree(CUdeviceptr ptr, int idx) {
    std::vector<ProcessUsageListener> listeners;
    ProcessUsage snapshot;
    bool updated = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (idx >= 0 &&
            idx < static_cast<int>(device_memory_blocks_.size()) &&
            idx < static_cast<int>(process_usage_.devices.size())) {
            auto& memory_blocks = device_memory_blocks_[idx];

            if (const auto it = memory_blocks.find(ptr); it != memory_blocks.end()) {
                const size_t freed_size = it->second.size;
                memory_blocks.erase(it);

                process_usage_.process_id = getpid();
                auto& usage = process_usage_.devices[idx];
                if (usage.usage >= freed_size) {
                    usage.usage -= freed_size;
                } else {
                    usage.usage = 0;
                }

                process_usage_.timestamp = std::time(nullptr);
                snapshot = process_usage_;
                listeners = process_usage_listeners_;
                updated = true;
            }
        }
    }

    if (updated) {
        for (auto& listener : listeners) {
            if (listener) {
                listener(snapshot);
            }
        }
    }
}

// get device memory usage
size_t Device::getDeviceMemoryUsage(int idx) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (likely(idx == DEVICE_INDEX_CURRENT)){
        idx = device_id_;
    }

    if (idx >= 0 && idx < static_cast<int>(process_usage_.devices.size())) {
        return process_usage_.getUsage(idx);
    }

    return 0;
}

size_t Device::getDeviceMemoryLimit(int _) const {
    return device_memory_limit_bytes_;
}


// update memory usage
void Device::updateMemoryUsage(const enum MemOperation operation, CUdeviceptr ptr, size_t size, int idx) {
    if (likely(idx == DEVICE_INDEX_CURRENT)){
        idx = device_id_;
    }

    if (operation == MemAlloc) {
        return recordAllocation(ptr, size, idx);
    } else {
        return recordFree(ptr, idx);
    }
}

// get device name
std::string Device::getDeviceName() const {
    return device_name_;
}

void Device::registerProcessUsageObserver(ProcessUsageListener listener) {
    ProcessUsage snapshot;
    ProcessUsageListener callback;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        process_usage_listeners_.push_back(std::move(listener));
        snapshot = process_usage_;
        callback = process_usage_listeners_.back();
    }

    if (callback) {
        callback(snapshot);
    }
}
