#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <mutex>
#include <map>
#include <vector>
#include <string>
#include <functional>
#include <ctime>
#include <unistd.h>
#include <cuda.h>

#include "util/util.hpp"

#define DEVICE_INDEX_CURRENT -1
#define DEVICE_MAX_NUM 8

enum MemOperation {MemAlloc,MemFree};

class Device {
public:
   Device();
   ~Device() = default;

    struct MemoryBlock {
        CUdeviceptr ptr;
        size_t size;
    };

    struct DeviceUsage{
        int device_id = 0;
        size_t usage = 0;
    };

    struct ProcessUsage{
        pid_t process_id = 0; // process id
        time_t timestamp = 0; // for process sync
        std::vector<DeviceUsage> devices; // device usage

        // constructor
        ProcessUsage() : process_id(getpid()), timestamp(time(nullptr)), devices(DEVICE_MAX_NUM){
            for (int i = 0; i < DEVICE_MAX_NUM; i++) {
                devices[i].device_id = i;
                devices[i].usage = 0;
            }
        }

        size_t getUsage(int device_id) const{
                    if (device_id >= 0 && device_id < static_cast<int>(devices.size())) {
                return devices[device_id].usage;
            }
            return 0;
        }
    };

    using ProcessUsageListener = std::function<void(const ProcessUsage&)>;
    
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

    void registerProcessUsageObserver(ProcessUsageListener listener);
private:
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // member variables
    int device_id_ = 0; // device id
    mutable std::mutex mutex_{}; 
    size_t device_memory_limit_bytes_ = 0; // 0 means unlimited
    std::string device_name_ = ""; // device name
    ProcessUsage process_usage_ {}; 
    std::vector<std::map<CUdeviceptr, MemoryBlock>> device_memory_blocks_ {};
    std::vector<ProcessUsageListener> process_usage_listeners_;
};


#endif