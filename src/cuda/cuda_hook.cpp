#include <dlfcn.h>
#include <cstring>
#include <iostream>

#include "spdlog/spdlog.h"
#include "util/logger.hpp"
#include "util/config.hpp"
#include "backend/local_backend.hpp"
#include "backend/remote_backend.hpp"
#include "cuda/cuda_hook.hpp"
#include "remote/remote_executor.hpp"

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

CUresult cuInit(unsigned int flags) {
    CudaHook& hook = CudaHook::getInstance();
    if (util::Config::executionMode() == util::Config::ExecutionMode::Remote) {
        std::string error;
        if (!remote::RemoteExecutor::instance().ensureConnected(&error)) {
            spdlog::error("remote cuInit failed: {}", error);
            return CUDA_ERROR_NOT_INITIALIZED;
        }
        return CUDA_SUCCESS;
    }

    if (!ensureCudaSymbol(hook.ori_cuInit, SYMBOL_STRING(cuInit))) {
        spdlog::error("Unable to resolve original cuInit");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    const CUresult result = hook.ori_cuInit(flags);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuInit failed", result);
    }

    return result;
}

CUresult cuMemAlloc(CUdeviceptr* dptr, size_t byteSize) {
    CudaHook& hook = CudaHook::getInstance();
    if (util::Config::executionMode() == util::Config::ExecutionMode::Remote) {
        return RemoteBackend::instance().cuMemAlloc(hook, dptr, byteSize);
    }
    return LocalBackend::instance().cuMemAlloc(hook, dptr, byteSize);
}

CUresult cuDeviceGet(CUdevice* device, int ordinal) {
    CudaHook& hook = CudaHook::getInstance();
    if (util::Config::executionMode() == util::Config::ExecutionMode::Remote) {
        std::string error;
        if (!remote::RemoteExecutor::instance().ensureConnected(&error)) {
            spdlog::error("remote cuDeviceGet failed: {}", error);
            return CUDA_ERROR_NOT_INITIALIZED;
        }
        const auto handshake = remote::RemoteExecutor::instance().handshakeInfo();
        if (!handshake || ordinal < 0 || ordinal >= handshake->device_count) {
            return CUDA_ERROR_INVALID_DEVICE;
        }
        *device = static_cast<CUdevice>(ordinal);
        hook.getDevice().setDeviceId(ordinal);
        return CUDA_SUCCESS;
    }

    if (!ensureCudaSymbol(hook.ori_cuDeviceGet, SYMBOL_STRING(cuDeviceGet))) {
        spdlog::error("Unable to resolve original cuDeviceGet");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    return hook.ori_cuDeviceGet(device, ordinal);
}

CUresult cuMemFree(CUdeviceptr dptr) {
    CudaHook& hook = CudaHook::getInstance();
    if (util::Config::executionMode() == util::Config::ExecutionMode::Remote) {
        return RemoteBackend::instance().cuMemFree(hook, dptr);
    }
    return LocalBackend::instance().cuMemFree(hook, dptr);
}

CUresult cuCtxGetDevice(CUdevice* device) {
    CudaHook& hook = CudaHook::getInstance();
    if (util::Config::executionMode() == util::Config::ExecutionMode::Remote) {
        return RemoteBackend::instance().cuCtxGetDevice(hook, device);
    }
    return LocalBackend::instance().cuCtxGetDevice(hook, device);
}

CUresult cuCtxSetCurrent(CUcontext ctx) {
    CudaHook& hook = CudaHook::getInstance();
    if (util::Config::executionMode() == util::Config::ExecutionMode::Remote) {
        return RemoteBackend::instance().cuCtxSetCurrent(hook, ctx);
    }
    return LocalBackend::instance().cuCtxSetCurrent(hook, ctx);
}
CUresult cuMemGetInfo(size_t* free, size_t* total) {
    CudaHook& hook = CudaHook::getInstance();
    if (util::Config::executionMode() == util::Config::ExecutionMode::Remote) {
        return RemoteBackend::instance().cuMemGetInfo(hook, free, total);
    }
    return LocalBackend::instance().cuMemGetInfo(hook, free, total);
}

CUresult cuDeviceTotalMem(size_t *bytes, CUdevice dev){
    CudaHook& hook = CudaHook::getInstance();
    if (util::Config::executionMode() == util::Config::ExecutionMode::Remote) {
        return RemoteBackend::instance().cuDeviceTotalMem(hook, bytes, dev);
    }
    return LocalBackend::instance().cuDeviceTotalMem(hook, bytes, dev);
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
