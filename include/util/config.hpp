#ifndef UTIL_CONFIG_HPP
#define UTIL_CONFIG_HPP

#include <string>

namespace util {

class Config {
public:
    // Returns configured memory limit (bytes) from config file or environment.
    static std::size_t memoryLimitBytes();

    // Returns configured target device name from config file or environment.
    static std::string targetDeviceName();

    // Returns configured OverSub Memory ratio.
    static double overSubRatio();

private:
    static std::string getEnv(const char* name);
    static std::size_t parseByteSize(const std::string& value);
    static double parseRatio(const std::string& value);
};

} // namespace util

#endif // UTIL_CONFIG_HPP
