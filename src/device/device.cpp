#include "device/device.hpp"
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
    if (auto limit = util::Config::memoryLimitBytes()) {
        device_memory_limit_bytes_ = *limit;
    }

    if (auto deviceName = util::Config::targetDeviceName()) {
        device_name_ = *deviceName;
    }
}


// record allocation action
void Device::recordAllocation(CUdeviceptr ptr, size_t size, int device_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (device_id >= device_memory_blocks_.size()) {
        device_memory_blocks_.resize(device_id + 1);
        device_usage_.resize(device_id + 1);
    }

    // update memory usage and store memory block
    device_usage_[device_id] += size;
    device_memory_blocks_[device_id][ptr] = MemoryBlock{ptr, size};
}

// record free action
void Device::recordFree(CUdeviceptr ptr, int device_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (device_id < device_memory_blocks_.size()) {
        auto& memory_blocks = device_memory_blocks_[device_id];

        if (const auto it = memory_blocks.find(ptr); it != memory_blocks.end()) {
            // update memory usage and remove memory block
            memory_blocks.erase(it);
            device_usage_[device_id] -= it->second.size;    
            return;
        }
    }
    return;
}

// get device usage
size_t Device::getDeviceUsage(int device_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (device_id < device_usage_.size()) {
        return device_usage_[device_id];
    }

    return 0;
}

// update memory usage
void Device::updateMemoryUsage(const int device_id, const enum MemOperation operation, CUdeviceptr ptr, size_t size) {
    if (operation == MemAlloc) {
        return recordAllocation(ptr, size, device_id);
    } else {
        return recordFree(ptr, device_id);
    }
}

// get device name
std::string Device::getDeviceName() const {
    return device_name_;
}