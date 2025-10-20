#include <dlfcn.h>
#include <cstring>
#include <iostream>

#include "spdlog/spdlog.h"
#include "util/logger.hpp"
#include "cuda/cuda_hook.hpp"

extern void* real_dlsym(void*, const char*);

namespace {
    struct LoggerInitializer {
        LoggerInitializer() {
            util::Logger::init();
        }
    };

    LoggerInitializer g_logger_initializer;
    template <typename FnPtr>
    bool ensureCudaSymbol(FnPtr& fn, const char* symbol_name) {
        if (fn) {
            return true;
        }

        void* handle = dlopen(CUDA_LIBRARY_SO, RTLD_LAZY | RTLD_LOCAL);
        if (!handle) {
            spdlog::error("dlopen {} failed while loading {}: {}", CUDA_LIBRARY_SO, symbol_name, dlerror());
            return false;
        }

        dlerror();
        void* symbol = real_dlsym(handle, symbol_name);
        const char* error = dlerror();
        dlclose(handle);

        if (error != nullptr || symbol == nullptr) {
            spdlog::error("real_dlsym failed to load {}: {}", symbol_name, error ? error : "unknown error");
            return false;
        }

        fn = reinterpret_cast<FnPtr>(symbol);
        spdlog::trace("Loaded original symbol {}", symbol_name);
        return true;
    }

    void logCudaError(CudaHook& hook, const char* context, CUresult code) {
        const char* error_string = nullptr;
        if (hook.ori_cuGetErrorString || ensureCudaSymbol(hook.ori_cuGetErrorString, SYMBOL_STRING(cuGetErrorString))) {
            if (hook.ori_cuGetErrorString(code, &error_string) != CUDA_SUCCESS) {
                error_string = nullptr;
            }
        }

        if (error_string) {
            spdlog::error("{}: {}", context, error_string);
        } else {
            spdlog::error("{} (code {})", context, static_cast<int>(code));
        }
    }
}

#pragma GCC visibility push(default)

CUresult cuGetProcAddress(const char* symbol, void** pfn, int cudaVersion, cuuint64_t flags, CUdriverProcAddressQueryResult* symbolStatus) {
    CudaHook& hook = CudaHook::getInstance();
    if (!ensureCudaSymbol(hook.ori_cuGetProcAddress_v2, SYMBOL_STRING(cuGetProcAddress))) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (std::strcmp(symbol, "cuGetProcAddress") == 0) {
        *pfn = HOOK_SYMBOL(&cuGetProcAddress);
        return CUDA_SUCCESS;
    }

    
    if (CudaHook::HookFuncInfo hookInfo = CudaHook::getHookedSymbol(symbol);hookInfo.hookedFunc) {
        auto result = hook.ori_cuGetProcAddress_v2(symbol, pfn, cudaVersion, flags, symbolStatus);
        if (result != CUDA_SUCCESS) {
            return result;
        }
        
        if (hookInfo.original) {
            hookInfo.original(hook, *pfn);
        }
        
        if (hookInfo.hookedFunc != NO_HOOK) {
            spdlog::debug("cuGetProcAddress Hook: {}", symbol);
            *pfn = hookInfo.hookedFunc;
        }

        return result;
    }
    
    if (hook.ori_cuGetProcAddress_v2) {
        return hook.ori_cuGetProcAddress_v2(symbol, pfn, cudaVersion, flags, symbolStatus);
    }
    
    return CUDA_ERROR_NOT_INITIALIZED;
}

CUresult cuInit(unsigned int Flags) {
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuInit, SYMBOL_STRING(cuInit))) {
        spdlog::error("Unable to resolve original cuInit");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    const CUresult result = hook.ori_cuInit(Flags);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuInit failed", result);
    }

    return result;
}

CUresult cuMemAlloc(CUdeviceptr* dptr, size_t byteSize) {
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuMemAlloc_v2, SYMBOL_STRING(cuMemAlloc))) {
        spdlog::error("Unable to resolve original cuMemAlloc");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if(auto limit = hook.getDevice().getDeviceMemoryLimit();limit > 0){
        if(hook.getDevice().getDeviceMemoryUsage() + byteSize > limit){
            spdlog::error("Out of memory, trying to allocate {} bytes, current usage {}", byteSize, hook.getDevice().getDeviceMemoryUsage());
            return CUDA_ERROR_OUT_OF_MEMORY;
        }
    }

    const CUresult result = hook.ori_cuMemAlloc_v2(dptr, byteSize);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuMemAlloc failed", result);
        return result;
    }

    hook.getDevice().updateMemoryUsage(MemAlloc,*dptr,byteSize);

    return result;
}

CUresult cuDeviceGet(CUdevice* device, int ordinal) {
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuDeviceGet, SYMBOL_STRING(cuDeviceGet))) {
        spdlog::error("Unable to resolve original cuDeviceGet");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    return hook.ori_cuDeviceGet(device, ordinal);
}

CUresult cuMemFree(CUdeviceptr dptr) {
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuMemFree_v2, SYMBOL_STRING(cuMemFree))) {
        spdlog::error("Unable to resolve original cuMemFree");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    const CUresult result = hook.ori_cuMemFree_v2(dptr);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuMemFree failed", result);
    }

    hook.getDevice().updateMemoryUsage(MemFree, dptr);

    return result;
}

CUresult cuCtxGetDevice(CUdevice* device) {
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuCtxGetDevice, SYMBOL_STRING(cuCtxGetDevice))) {
        spdlog::error("Unable to resolve original cuCtxGetDevice");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    return hook.ori_cuCtxGetDevice(device);
}

CUresult cuCtxSetCurrent(CUcontext ctx) {
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuCtxSetCurrent, SYMBOL_STRING(cuCtxSetCurrent))) {
        spdlog::error("Unable to resolve original cuCtxSetCurrent");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (!ensureCudaSymbol(hook.ori_cuCtxGetDevice, SYMBOL_STRING(cuCtxGetDevice))) {
        spdlog::error("Unable to resolve original cuCtxGetDevice");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    CUresult result = hook.ori_cuCtxSetCurrent(ctx);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuCtxSetCurrent failed", result);
        return result;
    }

    CUdevice device;
    result = hook.ori_cuCtxGetDevice(&device);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuCtxGetDevice failed", result);
        return result;
    }

    hook.getDevice().setDeviceId(int(device));

    return result;
}
CUresult cuMemGetInfo(size_t* free, size_t* total) {
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuMemGetInfo_v2, SYMBOL_STRING(cuMemGetInfo))) {
        spdlog::error("Unable to resolve original cuMemGetInfo");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (auto limit = hook.getDevice().getDeviceMemoryLimit(); limit > 0){
        *total = limit;
        *free = limit - hook.getDevice().getDeviceMemoryUsage();
        return CUDA_SUCCESS;
    }

    const CUresult result = hook.ori_cuMemGetInfo_v2(free, total);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuMemGetInfo failed", result);
        return result;
    }

    return result;
}

CUresult cuDeviceTotalMem(size_t *bytes, CUdevice dev){
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuDeviceTotalMem_v2, SYMBOL_STRING(cuDeviceTotalMem))) {
        spdlog::error("Unable to resolve original cuDeviceTotalMem");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (auto limit = hook.getDevice().getDeviceMemoryLimit(int(dev)); limit > 0){
        *bytes = limit;
        return CUDA_SUCCESS;
    }

    const CUresult result = hook.ori_cuDeviceTotalMem_v2(bytes, dev);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuDeviceTotalMem failed", result);
        return result;
    }

    return result;
}

CUresult cuMemCreate(CUmemGenericAllocationHandle* handle, size_t size, const CUmemAllocationProp* prop, unsigned long long flags){
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuMemCreate, SYMBOL_STRING(cuMemCreate))) {
        spdlog::error("Unable to resolve original cuMemCreate");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if(!prop){
        return CUDA_ERROR_INVALID_VALUE;
    }

    CUresult result = CUDA_SUCCESS;
    // if not alloc from device 
    if(prop->location.type != CU_MEM_LOCATION_TYPE_DEVICE){
        CUresult result = hook.ori_cuMemCreate(handle, size, prop, flags);
        if (result != CUDA_SUCCESS) {
            logCudaError(hook, "cuMemCreate failed", result);
        }

        return result;
    }

    int idx = prop->location.id;
    if(auto limit = hook.getDevice().getDeviceMemoryLimit(idx);limit > 0){
        if(hook.getDevice().getDeviceMemoryUsage(idx) + size > limit){
            spdlog::error("VMM Out of memory, trying to allocate {} bytes, current usage {}", size, hook.getDevice().getDeviceMemoryUsage(idx));
            return CUDA_ERROR_OUT_OF_MEMORY;
        }
    }

    result = hook.ori_cuMemCreate(handle, size, prop, flags);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuMemCreate failed", result);
        return result;
    }

    hook.getDevice().updateMemoryUsage(MemAlloc, reinterpret_cast<CUdeviceptr>(*handle), size, idx);
    return result;
}

CUresult cuMemRelease(CUmemGenericAllocationHandle handle){
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuMemRelease, SYMBOL_STRING(cuMemRelease))) {
        spdlog::error("Unable to resolve original cuMemRelease");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    CUresult result = hook.ori_cuMemRelease(handle);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuMemRelease failed", result);
        return result;
    }

    hook.getDevice().updateMemoryUsage(MemFree, reinterpret_cast<CUdeviceptr>(handle));
    return result;
}


#pragma GCC visibility pop