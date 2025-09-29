#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <mutex>
#include <map>
#include <vector>
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


    // todo:// impl me
	// get device usage
    size_t getDeviceUsage(int) const;

	// update memory usage
    void updateMemoryUsage(const int, const MemOperation, CUdeviceptr, size_t size = 0);

private:
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    mutable std::mutex mutex_;
    std::vector<std::map<CUdeviceptr, MemoryBlock>> device_memory_blocks_;
};


#endif