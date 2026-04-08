#include "util/config.hpp"

#include <algorithm>
#include <climits>
#include <limits>
#include <mutex>
#include <optional>
#include <string>

#include <yaml-cpp/yaml.h>

namespace util {
namespace {

constexpr const char* kMemoryLimitEnv = "VCUDA_MEMORY_LIMIT";
constexpr const char* kDeviceNameEnv = "VCUDA_DEVICE_NAME";
constexpr const char* kExecutionModeEnv = "VCUDA_EXECUTION_MODE";
constexpr const char* kRemoteAddressEnv = "VCUDA_REMOTE_ADDR";
constexpr const char* kRemoteTimeoutEnv = "VCUDA_REMOTE_TIMEOUT_MS";
constexpr const char* kRemoteRdmaPreferredEnv = "VCUDA_REMOTE_RDMA_PREFERRED";
constexpr const char* kConfigFilePath = "/etc/vcuda/config.yaml";

struct FileConfig {
    std::optional<std::size_t> memory_limit;
    std::optional<std::string> device_name;
    std::optional<std::string> execution_mode;
    std::optional<std::string> remote_address;
    std::optional<std::size_t> remote_timeout_ms;
    std::optional<bool> remote_rdma_preferred;
};

std::string trim(const std::string& input) {
    if (input.empty()) {
        return {};
    }

    size_t first = 0;
    while (first < input.size() && 
           (input[first] == ' ' || input[first] == '\t' || 
            input[first] == '\n' || input[first] == '\r')) {
        ++first;
    }

    if (first == input.size()) {
        return {};
    }

    size_t last = input.size() - 1;
    while (last > first && 
           (input[last] == ' ' || input[last] == '\t' || 
            input[last] == '\n' || input[last] == '\r')) {
        --last;
    }

    return input.substr(first, last - first + 1);
}

std::size_t parseUnsigned(const std::string& text) {
    const auto cleaned = trim(text);
    if (cleaned.empty()) {
        return 0;
    }

    const char* str = cleaned.c_str();
    unsigned long long number = 0;
    int i = 0;
    
    while (str[i] >= '0' && str[i] <= '9') {
        if (number > (ULLONG_MAX - (str[i] - '0')) / 10) {
            return 0;
        }
        number = number * 10 + (str[i] - '0');
        i++;
    }

    if (str[i] != '\0') {
        return 0;
    }

    if (number > std::numeric_limits<std::size_t>::max()) {
        return 0;
    }

    return static_cast<std::size_t>(number);
}

std::string toLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch + ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

bool parseBool(const std::string& value, bool default_value = false) {
    const auto normalized = toLowerCopy(trim(value));
    if (normalized.empty()) {
        return default_value;
    }
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }
    return default_value;
}

std::size_t multiplyWithOverflowCheck(std::size_t value, std::size_t multiplier) {
    if (value == 0 || multiplier == 0) {
        return static_cast<std::size_t>(0);
    }

    if (value > std::numeric_limits<std::size_t>::max() / multiplier) {
        return 0;
    }

    return value * multiplier;
}
std::size_t parseByteSizeInternal(const std::string& value) {
    auto trimmed = trim(value);
    if (trimmed.empty()) {
        return 0;
    }

    std::size_t suffixPos = trimmed.size();
    while (suffixPos > 0) {
        const unsigned char ch = static_cast<unsigned char>(trimmed[suffixPos - 1]);
         if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
            --suffixPos;
        } else {
            break;
        }
    }

    std::string numberPart = trim(trimmed.substr(0, suffixPos));
    std::string suffix = toLowerCopy(trimmed.substr(suffixPos));

    if (suffix == "b" || suffix == "byte" || suffix == "bytes") {
        suffix.clear();
    }

    std::size_t multiplier = 1;
    if (suffix.empty()) {
        multiplier = 1;
    } else if (suffix == "k" || suffix == "kb" || suffix == "ki") {
        multiplier = 1024ull;
    } else if (suffix == "m" || suffix == "mb" || suffix == "mi") {
        multiplier = 1024ull * 1024ull;
    } else if (suffix == "g" || suffix == "gb" || suffix == "gi") {
        multiplier = 1024ull * 1024ull * 1024ull;
    } else if (suffix == "t" || suffix == "tb" || suffix == "ti") {
        multiplier = 1024ull * 1024ull * 1024ull * 1024ull;
    } else {
        return 0;
    }

    auto baseValue = parseUnsigned(numberPart);
    if (!baseValue) {
        return 0;
    }

    return multiplyWithOverflowCheck(baseValue, multiplier);
}

FileConfig loadConfigFile() {
    FileConfig config;

    try {
        YAML::Node root = YAML::LoadFile(kConfigFilePath);
        if (!root || !root.IsMap()) {
            return config;
        }

            const auto loadMemory = [&](const YAML::Node& node) {
            if (!node) {
                return;
            }
            try {
                const std::string raw = node.as<std::string>();
                if (auto parsed = parseByteSizeInternal(raw)) {
                    config.memory_limit = parsed;
                }
            } catch (const YAML::Exception&) {
                // ignore invalid entries
            }
        };

        const auto loadDevice = [&](const YAML::Node& node) {
            if (!node) {
                return;
            }
            try {
                if (node.IsScalar()) {
                    config.device_name = node.as<std::string>();
                }
            } catch (const YAML::Exception&) {
                // ignore invalid entries
            }
        };

        const auto loadString = [](const YAML::Node& node, std::optional<std::string>& output) {
            if (!node) {
                return;
            }
            try {
                if (node.IsScalar()) {
                    output = node.as<std::string>();
                }
            } catch (const YAML::Exception&) {
            }
        };

        const auto loadUnsigned = [&](const YAML::Node& node, std::optional<std::size_t>& output) {
            if (!node) {
                return;
            }
            try {
                if (node.IsScalar()) {
                    const auto parsed = parseUnsigned(node.as<std::string>());
                    if (parsed > 0) {
                        output = parsed;
                    }
                }
            } catch (const YAML::Exception&) {
            }
        };

        const auto loadBool = [&](const YAML::Node& node, std::optional<bool>& output) {
            if (!node) {
                return;
            }
            try {
                if (node.IsScalar()) {
                    output = parseBool(node.as<std::string>());
                }
            } catch (const YAML::Exception&) {
            }
        };

        loadMemory(root["memory_limit"]);
        if (!config.memory_limit) {
            loadMemory(root["memoryLimit"]);
        }

        loadDevice(root["device_name"]);
        if (!config.device_name) {
            loadDevice(root["target_device"]);
        }
        if (!config.device_name) {
            loadDevice(root["target_device_name"]);
        }

        loadString(root["execution_mode"], config.execution_mode);
        if (!config.execution_mode) {
            loadString(root["executionMode"], config.execution_mode);
        }

        loadString(root["remote_address"], config.remote_address);
        if (!config.remote_address) {
            loadString(root["remoteAddress"], config.remote_address);
        }

        loadUnsigned(root["remote_timeout_ms"], config.remote_timeout_ms);
        if (!config.remote_timeout_ms) {
            loadUnsigned(root["remoteTimeoutMs"], config.remote_timeout_ms);
        }

        loadBool(root["remote_rdma_preferred"], config.remote_rdma_preferred);
        if (!config.remote_rdma_preferred) {
            loadBool(root["remoteRdmaPreferred"], config.remote_rdma_preferred);
        }

        if (config.memory_limit || config.device_name || config.execution_mode || config.remote_address ||
            config.remote_timeout_ms || config.remote_rdma_preferred) {
            return config;
        }
    } catch (const YAML::Exception&) {
        return config;
    }

    return config;
}

FileConfig& cachedFileConfig() {
    static std::once_flag flag;
    static FileConfig cache;
    std::call_once(flag, [] {
        cache = loadConfigFile();
    });
    return cache;
}

} // namespace

std::size_t Config::memoryLimitBytes() {
    if (const auto& fileCfg = cachedFileConfig(); fileCfg.memory_limit) {
        return fileCfg.memory_limit.value();
    }

    auto raw = getEnv(kMemoryLimitEnv);
    if (size(raw) == 0) {
        return 0;
    }

    return parseByteSize(raw);
}

std::string Config::targetDeviceName() {
    if (const auto& fileCfg = cachedFileConfig(); fileCfg.device_name) {
        return fileCfg.device_name.value();
    }

    return getEnv(kDeviceNameEnv);
}

Config::ExecutionMode Config::executionMode() {
    std::string value;
    if (const auto& fileCfg = cachedFileConfig(); fileCfg.execution_mode) {
        value = fileCfg.execution_mode.value();
    } else {
        value = getEnv(kExecutionModeEnv);
    }

    const auto normalized = toLowerCopy(trim(value));
    if (normalized == "remote") {
        return ExecutionMode::Remote;
    }
    return ExecutionMode::Local;
}

std::string Config::remoteAddress() {
    if (const auto& fileCfg = cachedFileConfig(); fileCfg.remote_address) {
        return fileCfg.remote_address.value();
    }
    return getEnv(kRemoteAddressEnv);
}

std::size_t Config::remoteTimeoutMs() {
    if (const auto& fileCfg = cachedFileConfig(); fileCfg.remote_timeout_ms) {
        return fileCfg.remote_timeout_ms.value();
    }
    const auto raw = getEnv(kRemoteTimeoutEnv);
    if (raw.empty()) {
        return 3000;
    }
    const auto parsed = parseUnsigned(raw);
    return parsed > 0 ? parsed : 3000;
}

bool Config::remoteRdmaPreferred() {
    if (const auto& fileCfg = cachedFileConfig(); fileCfg.remote_rdma_preferred) {
        return fileCfg.remote_rdma_preferred.value();
    }
    return parseBool(getEnv(kRemoteRdmaPreferredEnv), true);
}

std::string Config::getEnv(const char* name) {
    if (!name) {
        return "";
    }

    if (const char* value = std::getenv(name); value && *value) {
        return std::string(value);
    }

    return "";
}

std::size_t Config::parseByteSize(const std::string& value) {
    return parseByteSizeInternal(value);
}

} // namespace util


