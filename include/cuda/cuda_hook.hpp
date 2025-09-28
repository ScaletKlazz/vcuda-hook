#ifndef CUDA_HOOK_DEFINE
#define CUDA_HOOK_DEFINE

#include <cuda.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <unordered_set>
#include <map>

#include "util/util.hpp"

#define CUDA_LIBRARY_SO "libcuda.so"
#define DEBUG true

#define ADD_HOOKED_SYMBOL(symbol, hook_ptr) \
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
    if (DEBUG) { \
        result = call; \
        if (result != CUDA_SUCCESS) { \
            const char* errorString = nullptr; \
            hook.ori_cuGetErrorString(result, &errorString); \
            SPDLOG_LOGGER_ERROR(spdlog::get("cuda_logger"), "CUDA error: {}", errorString); \
        } \
    }

auto cuda_logger = spdlog::stdout_color_mt("cuda_logger");

class CudaHook {
public:
    struct HookFuncInfo {
        void* hookedFunc;
        std::function<void(CudaHook&, void*)> assignOriginal;
    };

    struct MemoryBlock {
        CUdeviceptr ptr;
        size_t size;
        int device_id;
    };

    static CudaHook& getInstance();

    static HookFuncInfo getHookedSymbol(const std::string& symbolName) {
       const auto& map = getHookMap();
       if (const auto it = map.find(symbolName); it != map.end()) {
           return it->second;
       }
       return { nullptr, nullptr };
    }
private:
    CudaHook() = default;
    CudaHook(const CudaHook&) = delete;
    CudaHook& operator=(const CudaHook&) = delete;
public:
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
            ADD_HOOKED_SYMBOL(cuGetProcAddress, reinterpret_cast<void*>(&cuGetProcAddress)),
            ADD_HOOKED_SYMBOL(cuMemAlloc, reinterpret_cast<void*>(&cuMemAlloc)),
            ADD_HOOKED_SYMBOL(cuDeviceGet, reinterpret_cast<void*>(&cuDeviceGet)),
            ADD_HOOKED_SYMBOL(cuInit, reinterpret_cast<void*>(&cuInit)),
            ADD_HOOKED_SYMBOL(cuGetErrorString, NO_HOOK),
            ADD_HOOKED_SYMBOL(cuMemFree, reinterpret_cast<void*>(&cuMemFree)),
            ADD_HOOKED_SYMBOL(cuCtxGetDevice, NO_HOOK),
            ADD_HOOKED_SYMBOL(cuPointerGetAttribute, NO_HOOK),
            ADD_HOOKED_SYMBOL(cuMemGetInfo, reinterpret_cast<void*>(&cuMemGetInfo)),
        };
        return map;
    }
};




#endif //  CUDA_HOOK_DEFINE

