#include <dlfcn.h>
#include <cstring>
#include <iostream>

#include "spdlog/spdlog.h"
#include "util/logger.hpp"
#include "util/util.hpp"
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
        if (hook.ori_cuGetErrorString || ensureCudaSymbol(hook.ori_cuGetErrorString, CUDA_SYMBOL_STRING(cuGetErrorString))) {
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

CUresult cuGetProcAddress(const char* symbol, void** pfn, int cudaVersion, cuuint64_t flags, CUdriverProcAddressQueryResult* symbolStatus) {
    CUresult result = CUDA_SUCCESS;
    CudaHook& hook = CudaHook::getInstance();
    if (!ensureCudaSymbol(hook.ori_cuGetProcAddress, CUDA_SYMBOL_STRING(cuGetProcAddress))) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    spdlog::debug("cuGetProcAddress request symbol {}", symbol);
    if (std::strcmp(symbol, "cuGetProcAddress") == 0) {
        *pfn = reinterpret_cast<void*>(&cuGetProcAddress);
        return CUDA_SUCCESS;
    }

    CudaHook::HookFuncInfo hookInfo = CudaHook::getHookedSymbol(symbol);
    if (hookInfo.hookedFunc) {
        result = hook.ori_cuGetProcAddress(symbol, pfn, cudaVersion, flags, symbolStatus);
        if (result != CUDA_SUCCESS) {
            return result;
        }
        
        if (hookInfo.original) {
            hookInfo.original(hook, *pfn);
        }
        
        if (hookInfo.hookedFunc != NO_HOOK) {
            *pfn = hookInfo.hookedFunc;
        }

        return result;
    }
    
    if (hook.ori_cuGetProcAddress) {
        return hook.ori_cuGetProcAddress(symbol, pfn, cudaVersion, flags, symbolStatus);
    }
    
    return CUDA_ERROR_NOT_INITIALIZED;
}

CUresult cuInit(unsigned int Flags) {
    spdlog::debug("cuInit request Flags {}", Flags);
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuInit, CUDA_SYMBOL_STRING(cuInit))) {
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

    if (!ensureCudaSymbol(hook.ori_cuMemAlloc, CUDA_SYMBOL_STRING(cuMemAlloc))) {
        spdlog::error("Unable to resolve original cuMemAlloc");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    const CUresult result = hook.ori_cuMemAlloc(dptr, byteSize);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuMemAlloc failed", result);
        return result;
    }

    if (!ensureCudaSymbol(hook.ori_cuPointerGetAttribute, CUDA_SYMBOL_STRING(cuPointerGetAttribute))) {
        spdlog::warn("Unable to resolve cuPointerGetAttribute - skipping device attribution for allocation");
        return result;
    }

    int device_id = -1;
    const CUresult attr_result = hook.ori_cuPointerGetAttribute(&device_id, CU_POINTER_ATTRIBUTE_DEVICE_ORDINAL, *dptr);
    if (attr_result != CUDA_SUCCESS) {
        logCudaError(hook, "cuPointerGetAttribute failed during cuMemAlloc", attr_result);
        return result;
    }

    hook.getDevice().updateMemoryUsage(MemAlloc,*dptr,byteSize);

    return result;
}

CUresult cuDeviceGet(CUdevice* device, int ordinal) {
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuDeviceGet, CUDA_SYMBOL_STRING(cuDeviceGet))) {
        spdlog::error("Unable to resolve original cuDeviceGet");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    return hook.ori_cuDeviceGet(device, ordinal);
}

CUresult cuMemFree(CUdeviceptr dptr) {
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuMemFree, CUDA_SYMBOL_STRING(cuMemFree))) {
        spdlog::error("Unable to resolve original cuMemFree");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    int device_id = -1;
    if (ensureCudaSymbol(hook.ori_cuPointerGetAttribute, CUDA_SYMBOL_STRING(cuPointerGetAttribute))) {
        const CUresult attr_result = hook.ori_cuPointerGetAttribute(&device_id, CU_POINTER_ATTRIBUTE_DEVICE_ORDINAL, dptr);
        if (attr_result != CUDA_SUCCESS) {
            logCudaError(hook, "cuPointerGetAttribute failed during cuMemFree", attr_result);
        }
    } else {
        spdlog::warn("Unable to resolve cuPointerGetAttribute - skipping device attribution for free");
    }

    const CUresult result = hook.ori_cuMemFree(dptr);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuMemFree failed", result);
    }

    hook.getDevice().updateMemoryUsage(MemFree, dptr, 0, device_id);

    return result;
}

CUresult cuCtxGetDevice(CUdevice* device) {
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuCtxGetDevice, CUDA_SYMBOL_STRING(cuCtxGetDevice))) {
        spdlog::error("Unable to resolve original cuCtxGetDevice");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    return hook.ori_cuCtxGetDevice(device);
}

CUresult cuCtxSetCurrent(CUcontext ctx) {
    CudaHook& hook = CudaHook::getInstance();

    if (!ensureCudaSymbol(hook.ori_cuCtxSetCurrent, CUDA_SYMBOL_STRING(cuCtxSetCurrent))) {
        spdlog::error("Unable to resolve original cuCtxSetCurrent");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (!ensureCudaSymbol(hook.ori_cuCtxGetDevice, CUDA_SYMBOL_STRING(cuCtxGetDevice))) {
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

    if (size_t limit = hook.getDevice().getDeviceMemoryLimit() > 0){
        *total = limit;
        *free = limit - hook.getDevice().getDeviceMemoryUsage();
        return CUDA_SUCCESS;
    }

    if (!ensureCudaSymbol(hook.ori_cuMemGetInfo, CUDA_SYMBOL_STRING(cuMemGetInfo))) {
        spdlog::error("Unable to resolve original cuMemGetInfo");
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    const CUresult result = hook.ori_cuMemGetInfo(free, total);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuMemGetInfo failed", result);
        return result;
    }

    return result;
}
