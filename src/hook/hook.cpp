#include <dlfcn.h>
#include <cstring>

#include "spdlog/spdlog.h"
#include "util/util.hpp"
#include "cuda/cuda_hook.hpp"
#include "nvml/nvml_hook.hpp"
#include "hook/hook.hpp"

void* real_dlsym(void* handle, const char* symbol) {
    using dlsym_t = void* (*)(void*, const char*);
    static dlsym_t r_dlsym = nullptr;

    if (!r_dlsym) {
        dlerror();
        r_dlsym = reinterpret_cast<dlsym_t>(dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5"));
        if (char* err = dlerror(); err != nullptr) {
            spdlog::error("Failed to get real dlsym: {}", err);
        }
    }

    if (!r_dlsym) {
        return nullptr;
    }

    return r_dlsym(handle, symbol);
}

namespace {
    template <typename HookT>
    void* tryHookSymbol(HookT& hook, const char* symbol, void* original_sym) {
        const auto hookInfo = HookT::getHookedSymbol(symbol);
        if (!hookInfo.hookedFunc || hookInfo.hookedFunc == NO_HOOK) {
            return original_sym;
        }

        if (original_sym && hookInfo.original) {
            hookInfo.original(hook, original_sym);
        }

        return hookInfo.hookedFunc;
    }
} 

// dlsym
// exported to external application
__attribute__((visibility("default"))) void* dlsym(void* handle, const char* symbol){
    if (!symbol) {
        return nullptr;
    }
    
    auto sym = real_dlsym(handle, symbol);
    spdlog::info("dlsym {} {}", symbol, sym);
    
    auto isCuda = strncmp(symbol, "cu", 2) == 0;
    auto isNvml = strncmp(symbol, "nvml", 4) == 0;

    if (!isCuda && !isNvml) {
        return sym;
    }

    if (isCuda){
        auto& hook = CudaHook::getInstance();
        return tryHookSymbol(hook, symbol, sym);
    }

    if (isNvml) {
        NvmlHook& hook = NvmlHook::getInstance();
        return tryHookSymbol(hook, symbol, sym);
    }


    return sym;
}
