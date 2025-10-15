#ifndef PROCESS_USAGE_H
#define PROCESS_USAGE_H

#include <ctime>
#include <array>
#include <unistd.h>
#include "util/util.hpp"

namespace util{
    struct DeviceUsage{
        int device_id = 0;
        size_t gpu_usage = 0;
    };

    struct ProcessUsage{
        pid_t process_id = 0; // process id
        time_t timestamp = 0; // for process sync
        std::array<DeviceUsage,DEVICE_MAX_NUM> devices; // device usage

        
        static ProcessUsage& getInstance(){
            static ProcessUsage instance;
            return instance;
        }

        void updateTimestamp(){
            timestamp = time(nullptr);
        }

        void clearUsage(){
            for(auto& device : devices){
                device.gpu_usage = 0;
            }
        }

        // update device usage
        void updateUsage(int device_id,size_t update_size){
            if(device_id >= 0 && device_id < DEVICE_MAX_NUM){
                devices[device_id].gpu_usage += update_size;

                if (devices[device_id].gpu_usage < 0){
                    devices[device_id].gpu_usage = 0;
                }
            }
            updateTimestamp();
        }

        // get device usage
        size_t getUsage(int device_id) const{
                    if (device_id >= 0 && device_id < static_cast<int>(devices.size())) {
                return devices[device_id].gpu_usage;
            }
    
            return 0;
        }

        // constructor
        ProcessUsage() : process_id(getpid()), timestamp(time(nullptr)){
            for (int i = 0; i < DEVICE_MAX_NUM; i++) {
                devices[i].device_id = i;
                devices[i].gpu_usage = 0;
            }
        }
    };
}

#endif