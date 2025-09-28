#include <dlfcn.h>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "util/util.hpp"
#include "cuda/cuda_hook.hpp"
#include "nvml/nvml_hook.hpp"
#include "client/client.hpp"

static void* real_dlsym(void* handle, const char* symbol) {
    typedef void* (dlsym_t)(void*, const char*);
    static dlsym_t* r_dlsym = nullptr;

    if (!r_dlsym) {
        dlerror();
        r_dlsym = reinterpret_cast<dlsym_t*>(dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5"));
        if (char *err = dlerror();err != nullptr) {
            SPDLOG_LOGGER_ERROR(spdlog::get("cuda_logger"), "Failed to get real dlsym: {}", err);
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
    // symbol compare
    // if (strcmp(symbol, CUDA_SYMBOL_STRING(cuGetProcAddress)) == 0) {
    //     return reinterpret_cast<void*>(&cuGetProcAddress);
    // }
    auto sym = real_dlsym(handle, symbol);
    fprintf(stderr, "dlsym: %s %p\n", symbol,sym);
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


        if (sym && hookInfo.assignOriginal) {
            hookInfo.assignOriginal(hook, sym);
        }
    }

    if (isNvml) {
        // NvmlHook& hook = NvmlHook::getInstance();
        // auto hookInfo = NvmlHook::getHookedSymbol(symbol);
        // if (!hookInfo.hookedFunc || hookInfo.hookedFunc == INTENTIONALLY_NOT_HOOKED) {
        //     return sym;
        // }

        // if (sym && hookInfo.assignOriginal) {
        //     hookInfo.assignOriginal(hook, sym);
        // }
    }


    return sym;
}