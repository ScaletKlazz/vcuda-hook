#ifndef CUDA_HOOK_DEFINE
#define CUDA_HOOK_DEFINE

#include <cuda.h>
#include "hook/hook.hpp"
#include "client/client.hpp"

#define CUDA_LIBRARY_SO "libcuda.so.1"

#define ADD_CUDA_SYMBOL(symbol, hook_ptr) \
    {#symbol, { \
        hook_ptr, \
        [](CudaHook& hook, void* ptr) { \
            hook.ori_##symbol = reinterpret_cast<CudaHook::symbol##_func_ptr>(ptr); \
        } \
    }}

#define ORI_CUDA_FUNC(name, return_type, ...) \
    using name##_func_ptr = return_type (*)(__VA_ARGS__); \
    name##_func_ptr ori_##name = nullptr;


class CudaHook : public BaseHook<CudaHook> {
public:
    // Symbols
    ORI_CUDA_FUNC(cuGetProcAddress, CUresult, const char*, void**, int, cuuint64_t, CUdriverProcAddressQueryResult*);
    ORI_CUDA_FUNC(cuMemAlloc, CUresult, CUdeviceptr*, size_t);
    ORI_CUDA_FUNC(cuDeviceGet, CUresult, CUdevice*, int);
    ORI_CUDA_FUNC(cuMemAllocHost, CUresult, void**, size_t);
    ORI_CUDA_FUNC(cuInit, CUresult, unsigned int);
    ORI_CUDA_FUNC(cuGetErrorString, CUresult, CUresult, const char**);
    ORI_CUDA_FUNC(cuMemFree, CUresult, CUdeviceptr);
    ORI_CUDA_FUNC(cuCtxGetDevice, CUresult, CUdevice*);
    ORI_CUDA_FUNC(cuCtxSetCurrent, CUresult, CUcontext);
    ORI_CUDA_FUNC(cuPointerGetAttribute, CUresult, void*, CUpointer_attribute, CUdeviceptr);
    ORI_CUDA_FUNC(cuMemGetInfo, CUresult, size_t*, size_t*);

    static const std::unordered_map<std::string, HookFuncInfo>& getHookMap() {
        static const std::unordered_map<std::string, HookFuncInfo> map = {
            ADD_CUDA_SYMBOL(cuGetProcAddress, HOOK_SYMBOL(&cuGetProcAddress)),
            ADD_CUDA_SYMBOL(cuMemAlloc, HOOK_SYMBOL(&cuMemAlloc)),
            ADD_CUDA_SYMBOL(cuDeviceGet, HOOK_SYMBOL(&cuDeviceGet)),
            ADD_CUDA_SYMBOL(cuInit, HOOK_SYMBOL(&cuInit)),
            ADD_CUDA_SYMBOL(cuGetErrorString, NO_HOOK),
            ADD_CUDA_SYMBOL(cuMemFree, HOOK_SYMBOL(&cuMemFree)),
            ADD_CUDA_SYMBOL(cuCtxGetDevice, HOOK_SYMBOL(&cuCtxGetDevice)),
            ADD_CUDA_SYMBOL(cuCtxSetCurrent, HOOK_SYMBOL(&cuCtxSetCurrent)),
            ADD_CUDA_SYMBOL(cuPointerGetAttribute, NO_HOOK),
            ADD_CUDA_SYMBOL(cuMemGetInfo, HOOK_SYMBOL(&cuMemGetInfo)),
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

