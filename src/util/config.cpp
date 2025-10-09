#include "util/config.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <limits>
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
    const auto first = input.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return {};
    }

    const auto last = input.find_last_not_of(" \t\n\r");
    return input.substr(first, last - first + 1);
}

std::string toLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::optional<std::size_t> parseUnsigned(const std::string& text) {
    const auto cleaned = trim(text);
    if (cleaned.empty()) {
        return std::nullopt;
    }

    unsigned long long number = 0;
    const char* begin = cleaned.data();
    const char* end = begin + cleaned.size();

    auto result = std::from_chars(begin, end, number);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }

    if (number > std::numeric_limits<std::size_t>::max()) {
        return std::nullopt;
    }

    return static_cast<std::size_t>(number);
}

std::optional<std::size_t> multiplyWithOverflowCheck(std::size_t value, std::size_t multiplier) {
    if (value == 0 || multiplier == 0) {
        return static_cast<std::size_t>(0);
    }

    if (value > std::numeric_limits<std::size_t>::max() / multiplier) {
        return std::nullopt;
    }

    return value * multiplier;
}
std::optional<std::size_t> parseByteSizeInternal(const std::string& value) {
    auto trimmed = trim(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    std::size_t suffixPos = trimmed.size();
    while (suffixPos > 0) {
        const unsigned char ch = static_cast<unsigned char>(trimmed[suffixPos - 1]);
        if (std::isalpha(ch)) {
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
        return std::nullopt;
    }

    auto baseValue = parseUnsigned(numberPart);
    if (!baseValue) {
        return std::nullopt;
    }

    return multiplyWithOverflowCheck(*baseValue, multiplier);
}

std::optional<FileConfig> loadConfigFile() {
    try {
        YAML::Node root = YAML::LoadFile(kConfigFilePath);
        if (!root || !root.IsMap()) {
            return std::nullopt;
        }

        FileConfig config;

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
        return std::nullopt;
    }

    return std::nullopt;
}

const std::optional<FileConfig>& cachedFileConfig() {
    static std::once_flag flag;
    static std::optional<FileConfig> cache;
    std::call_once(flag, [] {
        cache = loadConfigFile();
    });
    return cache;
}

} // namespace

std::optional<std::size_t> Config::memoryLimitBytes() {
    if (const auto& fileCfg = cachedFileConfig(); fileCfg && fileCfg->memory_limit) {
        return fileCfg->memory_limit;
    }

    auto raw = getEnv(kMemoryLimitEnv);
    if (!raw) {
        return std::nullopt;
    }

    return parseByteSize(*raw);
}

std::optional<std::string> Config::targetDeviceName() {
    if (const auto& fileCfg = cachedFileConfig(); fileCfg && fileCfg->device_name) {
        return fileCfg->device_name;
    }

    return getEnv(kDeviceNameEnv);
}

std::optional<std::string> Config::getEnv(const char* name) {
    if (!name) {
        return std::nullopt;
    }

    if (const char* value = std::getenv(name); value && *value) {
        return std::string(value);
    }

    return std::nullopt;
}

std::optional<std::size_t> Config::parseByteSize(const std::string& value) {
    return parseByteSizeInternal(value);
}

} // namespace util


