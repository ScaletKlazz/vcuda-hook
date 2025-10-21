#ifndef CUDA_HOOK_DEFINE
#define CUDA_HOOK_DEFINE
#include <cuda.h>
#include "hook/hook.hpp"
#include "client/client.hpp"
#include "util/util.hpp"

#define CUDA_LIBRARY_SO "libcuda.so.1"

#define ADD_CUDA_SYMBOL(symbol, hook_ptr) \
    {#symbol, { \
        hook_ptr, \
        [](CudaHook& hook, void* ptr) { \
            hook.CAT(ori_, EVAL(symbol)) = reinterpret_cast<CudaHook::CAT(EVAL(symbol), _func_ptr)>(ptr); \
        } \
    }}

#define MULTI_CUDA_SYMBOL(symbol, hook_ptr) \
    {#symbol, { \
        hook_ptr, \
        [](CudaHook& hook, void* ptr) { \
            hook.CAT(ori_, EVAL(symbol)) = reinterpret_cast<CudaHook::CAT(EVAL(symbol), _func_ptr)>(ptr); \
        } \
    }}, \
    {SYMBOL_STRING(symbol), { \
        hook_ptr, \
        [](CudaHook& hook, void* ptr) { \
            hook.CAT(ori_, EVAL(symbol)) = reinterpret_cast<CudaHook::CAT(EVAL(symbol), _func_ptr)>(ptr); \
        } \
    }}
      



class CudaHook : public BaseHook<CudaHook> {
public:
    // Symbols
    ORI_FUNC(cuGetProcAddress, CUresult, const char*, void**, int, cuuint64_t, CUdriverProcAddressQueryResult*);
    ORI_FUNC(cuMemAlloc, CUresult, CUdeviceptr*, size_t);
    ORI_FUNC(cuDeviceGet, CUresult, CUdevice*, int);
    ORI_FUNC(cuMemAllocHost, CUresult, void**, size_t);
    ORI_FUNC(cuInit, CUresult, unsigned int);
    ORI_FUNC(cuGetErrorString, CUresult, CUresult, const char**);
    ORI_FUNC(cuMemFree, CUresult, CUdeviceptr);
    ORI_FUNC(cuCtxGetDevice, CUresult, CUdevice*);
    ORI_FUNC(cuCtxSetCurrent, CUresult, CUcontext);
    ORI_FUNC(cuMemGetInfo, CUresult, size_t*, size_t*);
    ORI_FUNC(cuDeviceTotalMem, CUresult, size_t*, CUdevice);
    ORI_FUNC(cuMemGetAllocationGranularity, CUresult, size_t*, const CUmemAllocationProp*, CUmemAllocationGranularity_flags);
    ORI_FUNC(cuMemAddressReserve, CUresult, CUdeviceptr*, size_t, size_t, CUdeviceptr, unsigned long long);
    ORI_FUNC(cuMemAddressFree, CUresult, CUdeviceptr, size_t);
    ORI_FUNC(cuMemCreate, CUresult, CUmemGenericAllocationHandle*, size_t, const CUmemAllocationProp*, unsigned long long);
    ORI_FUNC(cuMemRelease, CUresult, CUmemGenericAllocationHandle);
    ORI_FUNC(cuMemMap, CUresult, CUdeviceptr, size_t, size_t, CUmemGenericAllocationHandle, unsigned long long);
    ORI_FUNC(cuMemUnmap,CUresult, CUdeviceptr, size_t);

    static const std::unordered_map<std::string, HookFuncInfo>& getHookMap() {
        static const std::unordered_map<std::string, HookFuncInfo> map = {
            MULTI_CUDA_SYMBOL(cuGetProcAddress, HOOK_SYMBOL(&cuGetProcAddress)),
            MULTI_CUDA_SYMBOL(cuMemAlloc, HOOK_SYMBOL(&cuMemAlloc)),
            ADD_CUDA_SYMBOL(cuDeviceGet, HOOK_SYMBOL(&cuDeviceGet)),
            MULTI_CUDA_SYMBOL(cuMemAllocHost, NO_HOOK),
            ADD_CUDA_SYMBOL(cuInit, HOOK_SYMBOL(&cuInit)),
            ADD_CUDA_SYMBOL(cuGetErrorString, NO_HOOK),
            MULTI_CUDA_SYMBOL(cuMemFree, HOOK_SYMBOL(&cuMemFree)),
            ADD_CUDA_SYMBOL(cuCtxGetDevice, HOOK_SYMBOL(&cuCtxGetDevice)),
            ADD_CUDA_SYMBOL(cuCtxSetCurrent, HOOK_SYMBOL(&cuCtxSetCurrent)),
            MULTI_CUDA_SYMBOL(cuMemGetInfo, HOOK_SYMBOL(&cuMemGetInfo)),
            MULTI_CUDA_SYMBOL(cuDeviceTotalMem, HOOK_SYMBOL(&cuDeviceTotalMem)),
            ADD_CUDA_SYMBOL(cuMemGetAllocationGranularity, NO_HOOK),
            ADD_CUDA_SYMBOL(cuMemAddressReserve, NO_HOOK),
            ADD_CUDA_SYMBOL(cuMemAddressFree, NO_HOOK),
            ADD_CUDA_SYMBOL(cuMemCreate, HOOK_SYMBOL(&cuMemCreate)),
            ADD_CUDA_SYMBOL(cuMemRelease, HOOK_SYMBOL(&cuMemRelease)),
            ADD_CUDA_SYMBOL(cuMemMap, NO_HOOK),
            ADD_CUDA_SYMBOL(cuMemUnmap, NO_HOOK),
        };
        return map;
    }

    const char* GetSymbolPrefix() const override {
        return symbolPrefixStr;
    }
private:
    friend class BaseHook<CudaHook>;
    CudaHook() = default;
    CudaHook(const CudaHook&) = delete;
    CudaHook& operator=(const CudaHook&) = delete;
protected:
    const char* symbolPrefixStr = "cu";    
};


#endif //  CUDA_HOOK_DEFINE

