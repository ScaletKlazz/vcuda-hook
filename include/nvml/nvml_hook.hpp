#ifndef NVML_HOOK_HPP
#define NVML_HOOK_HPP

#include <nvml.h>
#include "hook/hook.hpp"
#include "device/device.hpp"
#include "client/client.hpp"

#define NVML_LIBRARY_SO "libnvidia-ml.so"

#define ADD_NVML_SYMBOL(symbol, hook_ptr) \
    {#symbol, { \
        hook_ptr, \
        [](NvmlHook& hook, void* ptr) { \
            hook.ori_##symbol = reinterpret_cast<NvmlHook::symbol##_func_ptr>(ptr); \
        } \
    }}

#define ORI_NVML_FUNC(name, return_type, ...) \
    using name##_func_ptr = return_type (*)(__VA_ARGS__); \
    name##_func_ptr ori_##name = nullptr;

#define CHECK_NVML(call) \
        result = call; \
        if (result != NVML_SUCCESS) { \
            const char* errorString = nullptr; \
            hook.ori_nvmlErrorString(result, &errorString); \
            spdlog::error("NVML error: {}", errorString); \
        } 


class NvmlHook : public BaseHook<NvmlHook> {
public:
    // Symbols
    ORI_NVML_FUNC(nvmlErrorString, nvmlReturn_t, nvmlReturn_t, const char**);
    
    static const std::unordered_map<std::string, HookFuncInfo>& getHookMap() {
        static const std::unordered_map<std::string, HookFuncInfo> map = {
            ADD_NVML_SYMBOL(nvmlErrorString, NO_HOOK),
        };
        return map;
    }

private:
    friend class BaseHook<NvmlHook>;
    NvmlHook() = default;
    NvmlHook(const NvmlHook&) = delete;
    NvmlHook& operator=(const NvmlHook&) = delete; 
};

#endif