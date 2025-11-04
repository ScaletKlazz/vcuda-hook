#include <dlfcn.h>
#include <cstring>
#include <iostream>

#include "spdlog/spdlog.h"
#include "util/logger.hpp"
#include "nvml/nvml_hook.hpp"

extern void* real_dlsym(void* handle, const char* symbol);

namespace {
    struct LoggerInitializer {
        LoggerInitializer() {
            util::Logger::init();
        }
    };

    LoggerInitializer g_logger_initializer;
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
        spdlog::trace("Loaded original symbol {}", symbol_name);
        return true;
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

} // namespace

#pragma GCC visibility push(default)
nvmlReturn_t nvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_t* memory){
    auto& hook = NvmlHook::getInstance();

    if (!ensureNvmlSymbol(hook.ori_nvmlDeviceGetIndex, SYMBOL_STRING(nvmlDeviceGetIndex))) {
        spdlog::error("Unable to resolve original nvmlDeviceGetIndex");
        return NVML_ERROR_UNINITIALIZED ;
    }

    uint index = 0;
    auto result = hook.ori_nvmlDeviceGetIndex(device, &index);
    if (result != NVML_SUCCESS) {
        logNvmlError(hook, "nvmlDeviceGetIndex failed", result);
        return result;
    }

    if (size_t limit = hook.getDevice().getDeviceMemoryLimit();limit > 0){
        if (auto ratio = hook.getDevice().getDeviceOverSubRatio(); ratio > 0){
            limit = (1 + ratio) * limit;
        }
        
        memory->total = limit;
        memory->used = hook.getDevice().getDeviceMemoryUsage(int(index));
        memory->free = memory->total - memory->used;
        spdlog::trace("[nvmlDeviceGetMemoryInfo] Total: {}, Used: {}, Free: {}",
                      memory->total,
                      memory->used,
                      memory->free);
        return NVML_SUCCESS;
    }

    if (!ensureNvmlSymbol(hook.ori_nvmlDeviceGetMemoryInfo, SYMBOL_STRING(nvmlDeviceGetMemoryInfo))) {
        spdlog::error("Unable to resolve original nvmlDeviceGetMemoryInfo");
        return NVML_ERROR_UNINITIALIZED ;
    }

    result = hook.ori_nvmlDeviceGetMemoryInfo(device, memory);
    if (result != NVML_SUCCESS) {
        logNvmlError(hook, "nvmlDeviceGetMemoryInfo failed", result);
        return result;
    }

    return result;
}
nvmlReturn_t nvmlDeviceGetMemoryInfo_v2(nvmlDevice_t device, nvmlMemory_v2_t* memory){
    auto& hook = NvmlHook::getInstance();

    if (!ensureNvmlSymbol(hook.ori_nvmlDeviceGetIndex, SYMBOL_STRING(nvmlDeviceGetIndex))) {
        spdlog::error("Unable to resolve original nvmlDeviceGetIndex");
        return NVML_ERROR_UNINITIALIZED ;
    }

    uint index = 0;
    auto result = hook.ori_nvmlDeviceGetIndex(device, &index);
    if (result != NVML_SUCCESS) {
        logNvmlError(hook, "nvmlDeviceGetIndex failed", result);
        return result;
    }

    if (size_t limit = hook.getDevice().getDeviceMemoryLimit();limit > 0){
        if (auto ratio = hook.getDevice().getDeviceOverSubRatio(); ratio > 0){
            limit = (1 + ratio) * limit;
        }

        memory->total = limit;
        memory->used = hook.getDevice().getDeviceMemoryUsage(int(index));
        memory->free = memory->total - memory->used;
        spdlog::trace("[nvmlDeviceGetMemoryInfo_v2] Total: {}, Used: {}, Free: {}",
                      memory->total,
                      memory->used,
                      memory->free);
        return NVML_SUCCESS;
    }

    if (!ensureNvmlSymbol(hook.ori_nvmlDeviceGetMemoryInfo_v2, SYMBOL_STRING(nvmlDeviceGetMemoryInfo_v2))) {
        spdlog::error("Unable to resolve original nvmlDeviceGetMemoryInfo_v2");
        return NVML_ERROR_UNINITIALIZED ;
    }

    result = hook.ori_nvmlDeviceGetMemoryInfo_v2(device, memory);
    if (result != NVML_SUCCESS) {
        logNvmlError(hook, "nvmlDeviceGetMemoryInfo_v2 failed", result);
        return result;
    }

    return result;
}

nvmlReturn_t nvmlDeviceGetName(nvmlDevice_t device, char* name, unsigned int length){
    auto& hook = NvmlHook::getInstance();

    if (hook.getDevice().getDeviceName() != ""){
        strncpy(name, hook.getDevice().getDeviceName().c_str(), length);
        return NVML_SUCCESS;
    }

    if (!ensureNvmlSymbol(hook.ori_nvmlDeviceGetName, SYMBOL_STRING(nvmlDeviceGetName))) {
        spdlog::error("Unable to resolve original nvmlDeviceGetName");
        return NVML_ERROR_UNINITIALIZED ;
    }

    return hook.ori_nvmlDeviceGetName(device, name, length);
}


#pragma GCC visibility pop