#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <mutex>
#include <vector>


static void* real_dlsym(void* handle, const char* symbol);

extern "C" {
    __attribute__((visibility("default"))) void* dlsym(void* handle, const char* symbol);
}

enum MemOperation {MemAlloc,MemFree};

class Device {
public:
    struct MemoryBlock {
        CUdeviceptr ptr;
        size_t size;
    };

    void recordAllocation(CUdeviceptr ptr, size_t size, int device_id) {
       std::lock_guard<std::mutex> lock(mutex_);

       if (device_id >= device_memory_blocks_.size()) {
           device_memory_blocks_.resize(device_id + 1);
       }

       const MemoryBlock block{ptr, size};
       device_memory_blocks_[device_id][ptr] = block;
    }

    size_t recordFree(CUdeviceptr ptr, int device_id) {
       std::lock_guard<std::mutex> lock(mutex_);

       if (device_id < device_memory_blocks_.size()) {
           auto& memory_blocks = device_memory_blocks_[device_id];

           if (const auto it = memory_blocks.find(ptr); it != memory_blocks.end()) {
               size_t size = it->second.size;
               memory_blocks.erase(it);
               return size;
           }
       }
       return 0;
    }


    // todo:// impl me
	// get device usage
    size_t getDeviceUsage(int device_id) const {
       std::lock_guard<std::mutex> lock(mutex_);
       return 0;
    }

	// update memory usage
    void updateMemoryUsage(const int device_id, const MemOperation operation, CUdeviceptr ptr, size_t size = 0) {
       if (operation == MemAlloc) {
           recordAllocation(ptr, size, device_id);
       } else if (operation == MemFree) {
           recordFree(ptr, device_id);
       }
    }

private:
    Device() = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    mutable std::mutex mutex_;
    std::vector<std::map<CUdeviceptr, MemoryBlock>> device_memory_blocks_;
};

class Client {
public:

private:
    Client() = default;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    std::vector<Device> devices_;
};

#endif // CLIENT_HPP