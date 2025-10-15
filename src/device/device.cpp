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
Device::Device() :process_usage_(util::ProcessUsage::getInstance())
{ 
    if (auto limit = util::Config::memoryLimitBytes();limit > 0) {
        device_memory_limit_bytes_ = limit;
    }

    if (auto deviceName = util::Config::targetDeviceName();size(deviceName) > 0) {
        device_name_ = deviceName;
    }
}

Device::~Device() {}

void Device::setDeviceId(int idx) {
    device_id_ = idx;
}

int Device::getDeviceId() {
    return device_id_;
}


// record allocation action
void Device::recordAllocation(CUdeviceptr ptr, size_t size, int idx) {
        std::lock_guard<std::mutex> lock(mutex_);
        device_memory_blocks_[idx][ptr] = MemoryBlock{ptr, size};
        process_usage_.updateUsage(idx, size);
}

// record free action
void Device::recordFree(CUdeviceptr ptr, int idx) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (idx >= 0 &&
        idx < static_cast<int>(device_memory_blocks_.size())) {
        auto& memory_blocks = device_memory_blocks_[idx];

        if (const auto it = memory_blocks.find(ptr); it != memory_blocks.end()) {
            const size_t freed_size = it->second.size;
            memory_blocks.erase(it);

            // update memory usage
            process_usage_.updateUsage(idx, -freed_size);
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
        return Client::getInstance().get_device_process_metric_data(idx);
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
        recordAllocation(ptr, size, idx);
    } else {
        recordFree(ptr, idx);
    }

    Client::getInstance().update_process_metric_data(process_usage_);
}

// get device name
std::string Device::getDeviceName() const {
    return device_name_;
}
