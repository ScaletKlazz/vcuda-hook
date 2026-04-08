#include <dlfcn.h>
#include <cstring>
#include <iostream>

#include "spdlog/spdlog.h"
#include "util/logger.hpp"
#include "util/config.hpp"
#include "backend/local_backend.hpp"
#include "backend/remote_backend.hpp"
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
    if (util::Config::executionMode() == util::Config::ExecutionMode::Remote) {
        return RemoteBackend::instance().nvmlDeviceGetMemoryInfo(hook, device, memory);
    }
    return LocalBackend::instance().nvmlDeviceGetMemoryInfo(hook, device, memory);
}
nvmlReturn_t nvmlDeviceGetMemoryInfo_v2(nvmlDevice_t device, nvmlMemory_v2_t* memory){
    auto& hook = NvmlHook::getInstance();
    if (util::Config::executionMode() == util::Config::ExecutionMode::Remote) {
        return RemoteBackend::instance().nvmlDeviceGetMemoryInfo_v2(hook, device, memory);
    }
    return LocalBackend::instance().nvmlDeviceGetMemoryInfo_v2(hook, device, memory);
}

nvmlReturn_t nvmlDeviceGetName(nvmlDevice_t device, char* name, unsigned int length){
    auto& hook = NvmlHook::getInstance();
    if (util::Config::executionMode() == util::Config::ExecutionMode::Remote) {
        return RemoteBackend::instance().nvmlDeviceGetName(hook, device, name, length);
    }
    return LocalBackend::instance().nvmlDeviceGetName(hook, device, name, length);
}


#pragma GCC visibility pop
