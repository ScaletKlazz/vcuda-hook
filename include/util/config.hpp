#ifndef UTIL_CONFIG_HPP
#define UTIL_CONFIG_HPP

#include <optional>
#include <string>

namespace util {

class Config {
public:
    // Returns configured memory limit (bytes) from config file or environment.
    static std::optional<std::size_t> memoryLimitBytes();

    // Returns configured target device name from config file or environment.
    static std::optional<std::string> targetDeviceName();

private:
    static std::optional<std::string> getEnv(const char* name);
    static std::optional<std::size_t> parseByteSize(const std::string& value);
};

} // namespace util

#endif // UTIL_CONFIG_HPP
