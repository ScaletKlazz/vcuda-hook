#include <dlfcn.h>

#include "spdlog/spdlog.h"
#include "util/util.hpp"
#include "cuda/cuda_hook.hpp"
#include "nvml/nvml_hook.hpp"
#include "hook/hook.hpp"

static void* real_dlsym(void* handle, const char* symbol) {
    typedef void* (dlsym_t)(void*, const char*);
    static dlsym_t* r_dlsym = nullptr;

    if (!r_dlsym) {
        dlerror();
        r_dlsym = reinterpret_cast<dlsym_t*>(dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5"));
        if (char *err = dlerror();err != nullptr) {
            spdlog::error("Failed to get real dlsym: {}", err);
        }
    }

    if (!r_dlsym) {
        return nullptr;
    }

    return (*r_dlsym)(handle, symbol);
}

// dlsym
// exported to external application
__attribute__((visibility("default"))) void* dlsym(void* handle, const char* symbol){
    if (!symbol) {
        return nullptr;
    }
    
    auto sym = real_dlsym(handle, symbol);
    spdlog::info("dlsym {} {}", symbol, sym);
    // prefix cu compare

    auto isCuda = strncmp(symbol, "cu", 2) == 0;
    auto isNvml = strncmp(symbol, "nvml", 4) == 0;

    if ( !isCuda || !isNvml) {
        return sym;
    }

    if (isCuda){
        auto& hook = CudaHook::getInstance();
        auto hookInfo = CudaHook::getHookedSymbol(symbol);
        if (!hookInfo.hookedFunc || hookInfo.hookedFunc == NO_HOOK) {
            return sym;
        }


        if (sym && hookInfo.original) {
            hookInfo.original(hook, sym);
        }
    }

    if (isNvml) {
        NvmlHook& hook = NvmlHook::getInstance();
        auto hookInfo = NvmlHook::getHookedSymbol(symbol);
        if (!hookInfo.hookedFunc || hookInfo.hookedFunc == NO_HOOK) {
            return sym;
        }

        if (sym && hookInfo.original) {
            hookInfo.original(hook, sym);
        }
    }


    return sym;
}