#ifndef VCUDA_REMOTE_BACKEND_HPP
#define VCUDA_REMOTE_BACKEND_HPP

#include <cuda.h>
#include <nvml.h>

class CudaHook;
class NvmlHook;

class RemoteBackend {
public:
    static RemoteBackend& instance();

    CUresult cuMemAlloc(CudaHook& hook, CUdeviceptr* dptr, size_t byteSize);
    CUresult cuMemFree(CudaHook& hook, CUdeviceptr dptr);
    CUresult cuCtxGetDevice(CudaHook& hook, CUdevice* device);
    CUresult cuCtxSetCurrent(CudaHook& hook, CUcontext ctx);
    CUresult cuMemGetInfo(CudaHook& hook, size_t* free, size_t* total);
    CUresult cuDeviceTotalMem(CudaHook& hook, size_t* bytes, CUdevice dev);

    nvmlReturn_t nvmlDeviceGetMemoryInfo(NvmlHook& hook, nvmlDevice_t device, nvmlMemory_t* memory);
    nvmlReturn_t nvmlDeviceGetMemoryInfo_v2(NvmlHook& hook, nvmlDevice_t device, nvmlMemory_v2_t* memory);
    nvmlReturn_t nvmlDeviceGetName(NvmlHook& hook, nvmlDevice_t device, char* name, unsigned int length);

private:
    RemoteBackend() = default;
};

#endif  // VCUDA_REMOTE_BACKEND_HPP
