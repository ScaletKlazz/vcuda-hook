#include <dlfcn.h>
#include <cstring>

#include "spdlog/spdlog.h"
#include "util/logger.hpp"
#include "util/util.hpp"
#include "cuda/cuda_hook.hpp"
#include "nvml/nvml_hook.hpp"
#include "hook/hook.hpp"

namespace {
struct LoggerInitializer {
    LoggerInitializer() {
        util::Logger::init();
    }
};

LoggerInitializer g_logger_initializer;
} // namespace

void* real_dlsym(void* handle, const char* symbol) {
    using dlsym_t = void* (*)(void*, const char*);
    static dlsym_t r_dlsym = nullptr;

    util::Logger::init();

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

        spdlog::info("Hook {}", symbol);
        return hookInfo.hookedFunc;
    }

    template <typename HookT>
    bool matchSymbol(HookT& hook,const char* symbol) {
        return strncmp(symbol, hook.GetSymbolPrefix(), strlen(hook.GetSymbolPrefix())) == 0;
    }
} 

// dlsym
// exported to external application
__attribute__((visibility("default"))) void* dlsym(void* handle, const char* symbol){
    if (!symbol) {
        return nullptr;
    }
    
    auto sym = real_dlsym(handle, symbol);
    spdlog::debug("dlsym {} {}", symbol, sym);

    if (auto& hook = CudaHook::getInstance();matchSymbol(hook, symbol)){
        return tryHookSymbol(hook, symbol, sym);
    }

    if (auto& hook = NvmlHook::getInstance();matchSymbol(hook, symbol)) {
        return tryHookSymbol(hook, symbol, sym);
    }


    return sym;
}
