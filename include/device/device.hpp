#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <mutex>
#include <map>
#include <vector>
#include <string>
#include <cuda.h>

enum MemOperation {MemAlloc,MemFree};

class Device {
public:
   Device();
   ~Device() = default;


    struct MemoryBlock {
        CUdeviceptr ptr;
        size_t size;
    };

    void recordAllocation(CUdeviceptr, size_t, int);

    void recordFree(CUdeviceptr, int);

	// get device usage
    size_t getDeviceUsage(int) const;

	// update memory usage
    void updateMemoryUsage(const int, const MemOperation, CUdeviceptr, size_t size = 0);

    // get device name
    std::string getDeviceName() const;

private:
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // member variables
    size_t device_memory_limit_bytes_ = 0; // 0 means unlimited
    std::string device_name_ = ""; // device name
    mutable std::mutex mutex_;
    std::vector<size_t> device_usage_;
    std::vector<std::map<CUdeviceptr, MemoryBlock>> device_memory_blocks_;
};


#endif