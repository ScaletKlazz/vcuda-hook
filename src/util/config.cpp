#include "util/config.hpp"

#include <climits>
#include <mutex>
#include <optional>
#include <string>

#include <yaml-cpp/yaml.h>

namespace util {
namespace {

constexpr const char* kMemoryLimitEnv = "VCUDA_MEMORY_LIMIT";
constexpr const char* kDeviceNameEnv = "VCUDA_DEVICE_NAME";
constexpr const char* kConfigFilePath = "/etc/vcuda/config.yaml";

struct FileConfig {
    std::optional<std::size_t> memory_limit;
    std::optional<std::string> device_name;
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

        if (config.memory_limit || config.device_name) {
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


