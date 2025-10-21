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

// int real_ioctl(int fd, uint64_t request, void *arg) {
//     using ioctl_t = int (*)(int, uint64_t, void *);
//     static ioctl_t r_ioctl = nullptr;

//     util::Logger::init();

//     if (!r_ioctl) {
//         dlerror();
//         r_ioctl = reinterpret_cast<ioctl_t>(dlvsym(RTLD_NEXT, "ioctl", "GLIBC_2.2.5"));
//         if (char* err = dlerror(); err != nullptr) {
//             spdlog::error("Failed to get real ioctl: {}", err);
//         }
//     }

//     if (!r_ioctl) {
//         return EINVAL;
//     }

//     return r_ioctl(fd, request, arg);
// }

static int fd_path(int fd, char *buf, size_t sz) {
    char link[64];
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    ssize_t r = readlink(link, buf, sz-1);
    if (r < 0) { buf[0]=0; return -1;}
    buf[r]=0;
    return 0;
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

        spdlog::debug("Hook {}", symbol);
        return hookInfo.hookedFunc;
    }

    template <typename HookT>
    bool matchSymbol(HookT& hook,const char* symbol) {
        return strncmp(symbol, hook.GetSymbolPrefix(), strlen(hook.GetSymbolPrefix())) == 0;
    }
} 

// dlsym
// exported to external application
void* dlsym(void* handle, const char* symbol){
    if (!symbol) {
        return nullptr;
    }
    
    auto sym = real_dlsym(handle, symbol);
    spdlog::trace("Dlsym {}", symbol);

    if (auto& hook = CudaHook::getInstance();matchSymbol(hook, symbol)){
        return tryHookSymbol(hook, symbol, sym);
    }

    if (auto& hook = NvmlHook::getInstance();matchSymbol(hook, symbol)) {
        return tryHookSymbol(hook, symbol, sym);
    }


    return sym;
}

// ioctl
// exported to external application
// int ioctl(int fd, uint64_t request, void *arg){
//     char path[PATH_MAX];
//     if (fd_path(fd, path, sizeof(path)) == 0) {
//         if (strstr(path, "nvidia") != NULL) {
//             uint64_t cmd = _IOC_NR(request);
//             spdlog::debug("[ioctl-hook] pid={} fd={} path={} cmd={:x}\n", getpid(), fd, path, cmd);
//         }
//     }

//     return real_ioctl(fd, request, arg);
// }