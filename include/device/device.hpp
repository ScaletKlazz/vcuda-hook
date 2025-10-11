#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <mutex>
#include <map>
#include <vector>
#include <string>
#include <cuda.h>

#include "util/util.hpp"

#define DEVICE_INDEX_CURRENT -1

enum MemOperation {MemAlloc,MemFree};


class Device {
public:
   Device();
   ~Device() = default;


    struct MemoryBlock {
        CUdeviceptr ptr;
        size_t size;
    };

    void setDeviceId(int);
    
    int getDeviceId();

    void recordAllocation(CUdeviceptr, size_t, int);

    void recordFree(CUdeviceptr, int);

	// get device memory usage
    size_t getDeviceMemoryUsage(int idx = DEVICE_INDEX_CURRENT) const;

    // get device memory limit
    size_t getDeviceMemoryLimit(int idx = DEVICE_INDEX_CURRENT) const;

	// update memory usage
    void updateMemoryUsage(const MemOperation, CUdeviceptr, size_t size = 0, int idx = DEVICE_INDEX_CURRENT);

    // get device name
    std::string getDeviceName() const;

private:
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // member variables
    int device_id_ = 0; // device id
    mutable std::mutex mutex_{}; 
    size_t device_memory_limit_bytes_ = 0; // 0 means unlimited
    std::string device_name_ = ""; // device name
    std::vector<size_t> device_usage_ {};
    std::vector<std::map<CUdeviceptr, MemoryBlock>> device_memory_blocks_ {};
};


#endif