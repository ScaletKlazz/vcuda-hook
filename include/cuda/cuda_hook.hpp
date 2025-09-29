#ifndef CUDA_HOOK_DEFINE
#define CUDA_HOOK_DEFINE

#include <cuda.h>

#include "hook/hook.hpp"
#include "util/util.hpp"

#define CUDA_LIBRARY_SO "libcuda.so"

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

#define CHECK_CUDA(call) \
        result = call; \
        if (result != CUDA_SUCCESS) { \
            const char* errorString = nullptr; \
            hook.ori_cuGetErrorString(result, &errorString); \
            spdlog::error("CUDA error: {}", errorString); \
        }


class CudaHook : public BaseHook<CudaHook> {
public:
    // Symbols
    ORI_CUDA_FUNC(cuGetProcAddress, CUresult, const char*, void**, int, cuuint64_t, CUdriverProcAddressQueryResult*);
    ORI_CUDA_FUNC(cuMemAlloc, CUresult, CUdeviceptr*, size_t);
    ORI_CUDA_FUNC(cuDeviceGet, CUresult, CUdevice*, int);
    ORI_CUDA_FUNC(cuInit, CUresult, unsigned int);
    ORI_CUDA_FUNC(cuGetErrorString, CUresult, CUresult, const char**);
    ORI_CUDA_FUNC(cuMemFree, CUresult, CUdeviceptr);
    ORI_CUDA_FUNC(cuCtxGetDevice, CUresult, CUdevice*);
    ORI_CUDA_FUNC(cuPointerGetAttribute, CUresult, void*, CUpointer_attribute, CUdeviceptr);
    ORI_CUDA_FUNC(cuMemGetInfo, CUresult, size_t*, size_t*);

    static const std::unordered_map<std::string, HookFuncInfo>& getHookMap() {
        static const std::unordered_map<std::string, HookFuncInfo> map = {
            ADD_CUDA_SYMBOL(cuGetProcAddress, reinterpret_cast<void*>(&cuGetProcAddress)),
            ADD_CUDA_SYMBOL(cuMemAlloc, reinterpret_cast<void*>(&cuMemAlloc)),
            ADD_CUDA_SYMBOL(cuDeviceGet, reinterpret_cast<void*>(&cuDeviceGet)),
            ADD_CUDA_SYMBOL(cuInit, reinterpret_cast<void*>(&cuInit)),
            ADD_CUDA_SYMBOL(cuGetErrorString, NO_HOOK),
            ADD_CUDA_SYMBOL(cuMemFree, reinterpret_cast<void*>(&cuMemFree)),
            ADD_CUDA_SYMBOL(cuCtxGetDevice, NO_HOOK),
            ADD_CUDA_SYMBOL(cuPointerGetAttribute, NO_HOOK),
            ADD_CUDA_SYMBOL(cuMemGetInfo, reinterpret_cast<void*>(&cuMemGetInfo)),
        };
        return map;
    }

private:
    CudaHook() = default;
    CudaHook(const CudaHook&) = delete;
    CudaHook& operator=(const CudaHook&) = delete;
};


#endif //  CUDA_HOOK_DEFINE

