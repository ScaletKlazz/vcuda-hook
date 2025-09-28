#include <dlfcn.h>
#include <cstring>
#include <iostream>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "cuda/cuda_hook.hpp"

extern void* real_dlsym(void* handle, const char* symbol);

CudaHook& CudaHook::getInstance() {
   static auto instance = CudaHook{};
   return instance;
}

CUresult cuGetProcAddress(const char* symbol, void** pfn, int cudaVersion, cuuint64_t flags, CUdriverProcAddressQueryResult* symbolStatus) {
    CUresult result = CUDA_SUCCESS;
    CudaHook& hook = CudaHook::getInstance();
    if (!hook.ori_cuGetProcAddress) {
        void* handle = dlopen(CUDA_LIBRARY_SO, RTLD_LAZY);
        if (!handle) {
            SPDLOG_LOGGER_ERROR(spdlog::get("cuda_logger"), "dlopen failed: {}", dlerror());
            return CUDA_ERROR_NOT_INITIALIZED;
        }

        hook.ori_cuGetProcAddress = reinterpret_cast<CudaHook::cuGetProcAddress_func_ptr>(
            real_dlsym(handle, CUDA_SYMBOL_STRING(cuGetProcAddress)));
       
        dlclose(handle);
        if (!hook.ori_cuGetProcAddress) {
            SPDLOG_LOGGER_ERROR(spdlog::get("cuda_logger"), "real_dlsym failed to get cuGetProcAddress");
            return CUDA_ERROR_NOT_INITIALIZED;
        }
    }

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
        
        if (hookInfo.assignOriginal) {
            hookInfo.assignOriginal(hook, *pfn);
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
    SPDLOG_LOGGER_DEBUG(spdlog::get("cuda_logger"), "cuInit request Flags {}", Flags);
    CudaHook& hook = CudaHook::getInstance();

    if (!hook.ori_cuInit) {
        void* handle = dlopen(CUDA_LIBRARY_SO, RTLD_LAZY);
        if (handle) {
            hook.ori_cuInit = reinterpret_cast<CudaHook::cuInit_func_ptr>(real_dlsym(handle, CUDA_SYMBOL_STRING(cuInit)));
        }

        dlclose(handle);

        if (hook.ori_cuInit) {
            SPDLOG_LOGGER_DEBUG(spdlog::get("cuda_logger"), "get original cuInit success");
        } else {
            SPDLOG_LOGGER_ERROR(spdlog::get("cuda_logger"), "get original cuInit failed");
            return CUDA_ERROR_NOT_INITIALIZED;
        }
    }

    CUresult result = CUDA_SUCCESS;
    CHECK_CUDA(hook.ori_cuInit(Flags));

    return result;
}

CUresult cuMemAlloc(CUdeviceptr* dptr, size_t byteSize) {
    CudaHook& hook = CudaHook::getInstance();

    if (!hook.ori_cuMemAlloc) {
        void* handle = dlopen(CUDA_LIBRARY_SO, RTLD_LAZY);
        if (handle) {
            hook.ori_cuMemAlloc = reinterpret_cast<CudaHook::cuMemAlloc_func_ptr>(
                real_dlsym(handle, CUDA_SYMBOL_STRING(cuMemAlloc))
                );
        }

        dlclose(handle);

        if (hook.ori_cuMemAlloc) {
            SPDLOG_LOGGER_DEBUG(spdlog::get("cuda_logger"), "get original ori_cuMemAlloc success");
        } else {
            SPDLOG_LOGGER_ERROR(spdlog::get("cuda_logger"), "get original ori_cuMemAlloc failed");
            return CUDA_ERROR_NOT_INITIALIZED;
        }
    }

    CUresult result = CUDA_SUCCESS;
    CHECK_CUDA(hook.ori_cuMemAlloc(dptr, byteSize));

    int device_id;
    if (!hook.ori_cuPointerGetAttribute) {
        void* handle = dlopen(CUDA_LIBRARY_SO, RTLD_LAZY);
        if (handle) {
            hook.ori_cuPointerGetAttribute = reinterpret_cast<CudaHook::cuPointerGetAttribute_func_ptr>(
                real_dlsym(handle, CUDA_SYMBOL_STRING(cuPointerGetAttribute))
                );
        }

        dlclose(handle);

        if (hook.ori_cuPointerGetAttribute) {
            SPDLOG_LOGGER_DEBUG(spdlog::get("cuda_logger"), "get original ori_cuPointerGetAttribute success");
        } else {
            SPDLOG_LOGGER_ERROR(spdlog::get("cuda_logger"), "get original ori_cuPointerGetAttribute failed");
            return CUDA_ERROR_NOT_INITIALIZED;
        }
    }
    hook.ori_cuPointerGetAttribute(&device_id, CU_POINTER_ATTRIBUTE_DEVICE_ORDINAL, *dptr);
    // TODO: use client to update memory usage

    return result;
}

CUresult cuDeviceGet(CUdevice* device, int ordinal) {
    CudaHook& hook = CudaHook::getInstance();

    if (!hook.ori_cuDeviceGet) {
        void* handle = dlopen(CUDA_LIBRARY_SO, RTLD_LAZY);
        if (handle) {
            hook.ori_cuDeviceGet = reinterpret_cast<CudaHook::cuDeviceGet_func_ptr>(real_dlsym(handle, CUDA_SYMBOL_STRING(cuDeviceGet)));
        }

        dlclose(handle);

        if (hook.ori_cuDeviceGet) {
            SPDLOG_LOGGER_DEBUG(spdlog::get("cuda_logger"), "get original cuDeviceGet success");
        } else {
            SPDLOG_LOGGER_ERROR(spdlog::get("cuda_logger"), "get original cuDeviceGet failed");
            return CUDA_ERROR_NOT_INITIALIZED;
        }
    }

    return hook.ori_cuDeviceGet(device, ordinal);
}

CUresult cuMemFree(CUdeviceptr dptr) {
    CudaHook& hook = CudaHook::getInstance();

    if (!hook.ori_cuMemFree) {
        void* handle = dlopen(CUDA_LIBRARY_SO, RTLD_LAZY);
        if (handle) {
            hook.ori_cuMemFree = reinterpret_cast<CudaHook::cuMemFree_func_ptr>(real_dlsym(handle, CUDA_SYMBOL_STRING(cuMemFree)));
        }

        dlclose(handle);

        if (hook.ori_cuMemFree) {
            SPDLOG_LOGGER_DEBUG(spdlog::get("cuda_logger"), "get original cuMemFree success");
        } else {
            SPDLOG_LOGGER_ERROR(spdlog::get("cuda_logger"), "get original cuMemFree failed");
            return CUDA_ERROR_NOT_INITIALIZED;
        }
    }

    int device_id;
    hook.ori_cuPointerGetAttribute(&device_id, CU_POINTER_ATTRIBUTE_DEVICE_ORDINAL, dptr);
    // TODO: use client to update memory usage

    CUresult result = CUDA_SUCCESS;
    CHECK_CUDA(hook.ori_cuMemFree(dptr));

    return result;
}

CUresult cuMemGetInfo(size_t* free, size_t* total) {
    CudaHook& hook = CudaHook::getInstance();

    if (!hook.ori_cuMemGetInfo) {
        void* handle = dlopen(CUDA_LIBRARY_SO, RTLD_LAZY);
        if (handle) {
            hook.ori_cuMemGetInfo = reinterpret_cast<CudaHook::cuMemGetInfo_func_ptr>(real_dlsym(handle, CUDA_SYMBOL_STRING(cuMemGetInfo)));
        }

        dlclose(handle);

        if (hook.ori_cuMemGetInfo) {
            SPDLOG_LOGGER_DEBUG(spdlog::get("cuda_logger"), "get original cuMemGetInfo success");
        } else {
            SPDLOG_LOGGER_ERROR(spdlog::get("cuda_logger"), "get original cuMemGetInfo failed");
            return CUDA_ERROR_NOT_INITIALIZED;
        }
    }

    CUresult result = CUDA_SUCCESS;
    CHECK_CUDA(hook.ori_cuMemGetInfo(free, total));

    int device_id;
    hook.ori_cuCtxGetDevice(&device_id);
    SPDLOG_LOGGER_DEBUG(spdlog::get("cuda_logger"), "Device {} free: {} total: {}", device_id, *free, *total);

	// TODO: use client to get memory usage
    return result;
}