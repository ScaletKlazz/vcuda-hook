#ifndef UTIL_CONFIG_HPP
#define UTIL_CONFIG_HPP

#include <string>

namespace util {

class Config {
public:
    enum class ExecutionMode {
        Local,
        Remote,
    };

    // Returns configured memory limit (bytes) from config file or environment.
    static std::size_t memoryLimitBytes();

    // Returns configured target device name from config file or environment.
    static std::string targetDeviceName();

    // Returns execution mode. Defaults to local.
    static ExecutionMode executionMode();

    // Returns remote server address in host:port form.
    static std::string remoteAddress();

    // Returns remote request timeout in milliseconds.
    static std::size_t remoteTimeoutMs();

    // Returns whether RDMA/UCX should be preferred.
    static bool remoteRdmaPreferred();

private:
    static std::string getEnv(const char* name);
    static std::size_t parseByteSize(const std::string& value);
};

} // namespace util

#endif // UTIL_CONFIG_HPP
