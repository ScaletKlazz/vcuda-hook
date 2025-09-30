#include "device/device.hpp"


Device::Device()
{ 
}


void Device::recordAllocation(CUdeviceptr ptr, size_t size, int device_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (device_id >= device_memory_blocks_.size()) {
        device_memory_blocks_.resize(device_id + 1);
    }

    const MemoryBlock block{ptr, size};
    device_memory_blocks_[device_id][ptr] = block;
}

void Device::recordFree(CUdeviceptr ptr, int device_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (device_id < device_memory_blocks_.size()) {
        auto& memory_blocks = device_memory_blocks_[device_id];

        if (const auto it = memory_blocks.find(ptr); it != memory_blocks.end()) {
            memory_blocks.erase(it);
            return;
        }
    }
    return;
}

// get device usage
size_t Device::getDeviceUsage(int device_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
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