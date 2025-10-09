#ifndef HOOK_HPP
#define HOOK_HPP
#include <string>
#include <unordered_map>
#include <functional>

void* real_dlsym(void*, const char*);

extern "C" {
    __attribute__((visibility("default"))) void* dlsym(void* handle, const char* symbol);
}

#define NO_HOOK reinterpret_cast<void*>(static_cast<intptr_t>(-1))

template<typename Derived>
class BaseHook {
public:
    struct HookFuncInfo {
        void* hookedFunc;
        std::function<void(Derived&, void*)> original;
    };

    static Derived& getInstance() {
        static auto instance = Derived{};
        return instance;
    }

    static HookFuncInfo getHookedSymbol(const std::string& symbolName) {
        const auto& map = Derived::getHookMap();
        if (auto it = map.find(symbolName);it != map.end()) {
            return it->second;
        }
        return { nullptr, nullptr };
    }

    virtual const char* GetSymbolPrefix() const = 0;
protected:
    BaseHook() = default;
    BaseHook(const BaseHook&) = delete;
    BaseHook& operator=(const BaseHook&) = delete;

    // class var member
    char* symbolPrefixStr = nullptr;
};

#endif
