#ifndef NVML_HOOK_HPP
#define NVML_HOOK_HPP

#include <nvml.h>
#include "hook/hook.hpp"
#include "client/client.hpp"

#define NVML_LIBRARY_SO "libnvidia-ml.so.1"

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


class NvmlHook : public BaseHook<NvmlHook> {
public:
    // Symbols
    ORI_NVML_FUNC(nvmlErrorString, nvmlReturn_t, nvmlReturn_t, const char**);
    ORI_NVML_FUNC(nvmlDeviceGetMemoryInfo, nvmlReturn_t, nvmlDevice_t, nvmlMemory_t*);
    ORI_NVML_FUNC(nvmlDeviceGetMemoryInfo_v2, nvmlReturn_t, nvmlDevice_t, nvmlMemory_v2_t*);
    ORI_NVML_FUNC(nvmlDeviceGetName, nvmlReturn_t, nvmlDevice_t, char*, unsigned int);

    
    static const std::unordered_map<std::string, HookFuncInfo>& getHookMap() {
        static const std::unordered_map<std::string, HookFuncInfo> map = {
            ADD_NVML_SYMBOL(nvmlErrorString, NO_HOOK),
            ADD_NVML_SYMBOL(nvmlDeviceGetMemoryInfo, HOOK_SYMBOL(&nvmlDeviceGetMemoryInfo)),
            ADD_NVML_SYMBOL(nvmlDeviceGetMemoryInfo_v2, HOOK_SYMBOL(&nvmlDeviceGetMemoryInfo_v2)),
            ADD_NVML_SYMBOL(nvmlDeviceGetName, HOOK_SYMBOL(&nvmlDeviceGetName)),
        };
        return map;
    }

    const char* GetSymbolPrefix() const override {
        return symbolPrefixStr;
    }
private:
    friend class BaseHook<NvmlHook>;
    NvmlHook() = default;
    NvmlHook(const NvmlHook&) = delete;
    NvmlHook& operator=(const NvmlHook&) = delete; 
protected:
    const char* symbolPrefixStr = "nvml";   
};

#endif