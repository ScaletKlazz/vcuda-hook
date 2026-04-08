#include "backend/local_backend.hpp"

#include <cstring>
#include <dlfcn.h>

#include "cuda/cuda_hook.hpp"
#include "nvml/nvml_hook.hpp"
#include "hook/hook.hpp"
#include "spdlog/spdlog.h"

namespace {

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
    return true;
}

template <typename FnPtr>
bool ensureNvmlSymbol(FnPtr& fn, const char* symbol_name) {
    if (fn) {
        return true;
    }

    void* handle = dlopen(NVML_LIBRARY_SO, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        spdlog::error("dlopen {} failed while loading {}: {}", NVML_LIBRARY_SO, symbol_name, dlerror());
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

void logNvmlError(NvmlHook& hook, const char* context, nvmlReturn_t code) {
    const char* error_string = nullptr;
    if (hook.ori_nvmlErrorString || ensureNvmlSymbol(hook.ori_nvmlErrorString, SYMBOL_STRING(nvmlErrorString))) {
        if (hook.ori_nvmlErrorString(code, &error_string) != NVML_SUCCESS) {
            error_string = nullptr;
        }
    }

    if (error_string) {
        spdlog::error("{}: {}", context, error_string);
    } else {
        spdlog::error("{} (code {})", context, static_cast<int>(code));
    }
}

}  // namespace

LocalBackend& LocalBackend::instance() {
    static LocalBackend backend;
    return backend;
}

CUresult LocalBackend::cuMemAlloc(CudaHook& hook, CUdeviceptr* dptr, size_t byteSize) {
    if (!ensureCudaSymbol(hook.ori_cuMemAlloc_v2, SYMBOL_STRING(cuMemAlloc))) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }
    if (auto limit = hook.getDevice().getDeviceMemoryLimit(); limit > 0 &&
        hook.getDevice().getDeviceMemoryUsage() + byteSize > limit) {
        return CUDA_ERROR_OUT_OF_MEMORY;
    }

    const CUresult result = hook.ori_cuMemAlloc_v2(dptr, byteSize);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuMemAlloc failed", result);
        return result;
    }
    hook.getDevice().updateMemoryUsage(MemAlloc, *dptr, byteSize);
    return result;
}

CUresult LocalBackend::cuMemFree(CudaHook& hook, CUdeviceptr dptr) {
    if (!ensureCudaSymbol(hook.ori_cuMemFree_v2, SYMBOL_STRING(cuMemFree))) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }
    const CUresult result = hook.ori_cuMemFree_v2(dptr);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuMemFree failed", result);
        return result;
    }
    hook.getDevice().updateMemoryUsage(MemFree, dptr);
    return result;
}

CUresult LocalBackend::cuCtxGetDevice(CudaHook& hook, CUdevice* device) {
    if (!ensureCudaSymbol(hook.ori_cuCtxGetDevice, SYMBOL_STRING(cuCtxGetDevice))) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }
    const CUresult result = hook.ori_cuCtxGetDevice(device);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuCtxGetDevice failed", result);
        return result;
    }
    hook.getDevice().setDeviceId(int(*device));
    return result;
}

CUresult LocalBackend::cuCtxSetCurrent(CudaHook& hook, CUcontext ctx) {
    if (!ensureCudaSymbol(hook.ori_cuCtxSetCurrent, SYMBOL_STRING(cuCtxSetCurrent)) ||
        !ensureCudaSymbol(hook.ori_cuCtxGetDevice, SYMBOL_STRING(cuCtxGetDevice))) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }
    const CUresult result = hook.ori_cuCtxSetCurrent(ctx);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuCtxSetCurrent failed", result);
        return result;
    }
    CUdevice device{};
    const CUresult device_result = hook.ori_cuCtxGetDevice(&device);
    if (device_result != CUDA_SUCCESS) {
        logCudaError(hook, "cuCtxGetDevice failed", device_result);
        return device_result;
    }
    hook.getDevice().setDeviceId(int(device));
    return CUDA_SUCCESS;
}

CUresult LocalBackend::cuMemGetInfo(CudaHook& hook, size_t* free, size_t* total) {
    if (!ensureCudaSymbol(hook.ori_cuMemGetInfo_v2, SYMBOL_STRING(cuMemGetInfo))) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }
    if (auto limit = hook.getDevice().getDeviceMemoryLimit(); limit > 0) {
        *total = limit;
        *free = limit - hook.getDevice().getDeviceMemoryUsage();
        return CUDA_SUCCESS;
    }
    const CUresult result = hook.ori_cuMemGetInfo_v2(free, total);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuMemGetInfo failed", result);
    }
    return result;
}

CUresult LocalBackend::cuDeviceTotalMem(CudaHook& hook, size_t* bytes, CUdevice dev) {
    if (!ensureCudaSymbol(hook.ori_cuDeviceTotalMem_v2, SYMBOL_STRING(cuDeviceTotalMem))) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }
    if (auto limit = hook.getDevice().getDeviceMemoryLimit(int(dev)); limit > 0) {
        *bytes = limit;
        return CUDA_SUCCESS;
    }
    const CUresult result = hook.ori_cuDeviceTotalMem_v2(bytes, dev);
    if (result != CUDA_SUCCESS) {
        logCudaError(hook, "cuDeviceTotalMem failed", result);
    }
    return result;
}

nvmlReturn_t LocalBackend::nvmlDeviceGetMemoryInfo(NvmlHook& hook, nvmlDevice_t device, nvmlMemory_t* memory) {
    if (!ensureNvmlSymbol(hook.ori_nvmlDeviceGetIndex, SYMBOL_STRING(nvmlDeviceGetIndex))) {
        return NVML_ERROR_UNINITIALIZED;
    }
    unsigned int index = 0;
    auto result = hook.ori_nvmlDeviceGetIndex(device, &index);
    if (result != NVML_SUCCESS) {
        logNvmlError(hook, "nvmlDeviceGetIndex failed", result);
        return result;
    }
    if (const size_t limit = hook.getDevice().getDeviceMemoryLimit(); limit > 0) {
        memory->total = limit;
        memory->used = hook.getDevice().getDeviceMemoryUsage(int(index));
        memory->free = memory->total - memory->used;
        return NVML_SUCCESS;
    }
    if (!ensureNvmlSymbol(hook.ori_nvmlDeviceGetMemoryInfo, SYMBOL_STRING(nvmlDeviceGetMemoryInfo))) {
        return NVML_ERROR_UNINITIALIZED;
    }
    result = hook.ori_nvmlDeviceGetMemoryInfo(device, memory);
    if (result != NVML_SUCCESS) {
        logNvmlError(hook, "nvmlDeviceGetMemoryInfo failed", result);
    }
    return result;
}

nvmlReturn_t LocalBackend::nvmlDeviceGetMemoryInfo_v2(NvmlHook& hook, nvmlDevice_t device, nvmlMemory_v2_t* memory) {
    if (!ensureNvmlSymbol(hook.ori_nvmlDeviceGetIndex, SYMBOL_STRING(nvmlDeviceGetIndex))) {
        return NVML_ERROR_UNINITIALIZED;
    }
    unsigned int index = 0;
    auto result = hook.ori_nvmlDeviceGetIndex(device, &index);
    if (result != NVML_SUCCESS) {
        logNvmlError(hook, "nvmlDeviceGetIndex failed", result);
        return result;
    }
    if (const size_t limit = hook.getDevice().getDeviceMemoryLimit(); limit > 0) {
        memory->total = limit;
        memory->used = hook.getDevice().getDeviceMemoryUsage(int(index));
        memory->free = memory->total - memory->used;
        return NVML_SUCCESS;
    }
    if (!ensureNvmlSymbol(hook.ori_nvmlDeviceGetMemoryInfo_v2, SYMBOL_STRING(nvmlDeviceGetMemoryInfo_v2))) {
        return NVML_ERROR_UNINITIALIZED;
    }
    result = hook.ori_nvmlDeviceGetMemoryInfo_v2(device, memory);
    if (result != NVML_SUCCESS) {
        logNvmlError(hook, "nvmlDeviceGetMemoryInfo_v2 failed", result);
    }
    return result;
}

nvmlReturn_t LocalBackend::nvmlDeviceGetName(NvmlHook& hook, nvmlDevice_t device, char* name, unsigned int length) {
    (void) device;
    if (!hook.getDevice().getDeviceName().empty()) {
        std::strncpy(name, hook.getDevice().getDeviceName().c_str(), length);
        return NVML_SUCCESS;
    }
    if (!ensureNvmlSymbol(hook.ori_nvmlDeviceGetName, SYMBOL_STRING(nvmlDeviceGetName))) {
        return NVML_ERROR_UNINITIALIZED;
    }
    return hook.ori_nvmlDeviceGetName(device, name, length);
}
