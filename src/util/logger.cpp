#include "util/logger.hpp"

#include <mutex>
#include <string>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/null_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace util {
namespace {

constexpr const char* kLevelEnvVar = "VCUDA_LOG_LEVEL";
constexpr const char* kSinkEnvVar = "VCUDA_LOG_SINK";
constexpr const char* kFileEnvVar = "VCUDA_LOG_FILE";
constexpr const char* kLoggerName = "vcuda-hook";
constexpr const char* kPattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v";

std::string toLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch + ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

} // namespace

void Logger::init() {
    static std::once_flag flag;
    std::call_once(flag, [] {
        const char* level_env = std::getenv(kLevelEnvVar);
        const char* sink_env = std::getenv(kSinkEnvVar);

        const auto level = parseLevel(level_env);

        std::shared_ptr<spdlog::logger> logger;
        try {
            logger = buildLogger(sink_env);
        } catch (const spdlog::spdlog_ex& ex) {
            auto fallback_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
            auto fallback = std::make_shared<spdlog::logger>(kLoggerName, std::move(fallback_sink));
            fallback->set_pattern(kPattern);
            fallback->set_level(level);
            spdlog::set_default_logger(fallback);
            spdlog::set_level(level);
            spdlog::flush_on(spdlog::level::warn);
            spdlog::error("Logger initialization failed ({}); falling back to stderr", ex.what());
            return;
        }

        logger->set_pattern(kPattern);
        logger->set_level(level);
        spdlog::set_default_logger(logger);
        spdlog::set_level(level);
        spdlog::flush_on(spdlog::level::warn);
    });
}

spdlog::level::level_enum Logger::parseLevel(const char* value) {
    if (!value) {
        return spdlog::level::info;
    }

    const auto lowered = toLowerCopy(value);
    if (lowered == "trace") {
        return spdlog::level::trace;
    }
    if (lowered == "debug") {
        return spdlog::level::debug;
    }
    if (lowered == "info") {
        return spdlog::level::info;
    }
    if (lowered == "warn" || lowered == "warning") {
        return spdlog::level::warn;
    }
    if (lowered == "error" || lowered == "err") {
        return spdlog::level::err;
    }
    if (lowered == "critical" || lowered == "crit" || lowered == "fatal") {
        return spdlog::level::critical;
    }
    if (lowered == "off" || lowered == "none") {
        return spdlog::level::off;
    }

    return spdlog::level::info;
}

std::shared_ptr<spdlog::logger> Logger::buildLogger(const char* sink_value) {
    const std::string sink_setting = sink_value ? sink_value : "";
    const std::string sink_lower = toLowerCopy(sink_setting);

    if (sink_lower.empty() || sink_lower == "stderr") {
        auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        return std::make_shared<spdlog::logger>(kLoggerName, std::move(sink));
    }

    if (sink_lower == "stdout" || sink_lower == "console") {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        return std::make_shared<spdlog::logger>(kLoggerName, std::move(sink));
    }

    if (sink_lower == "null" || sink_lower == "none") {
        auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
        return std::make_shared<spdlog::logger>(kLoggerName, std::move(sink));
    }

    if (sink_lower.rfind("file", 0) == 0) {
        std::string file_path;
        if (sink_setting.size() > 5 && sink_setting[4] == ':') {
            file_path = sink_setting.substr(5);
        }

        if (file_path.empty()) {
            if (const char* path_env = std::getenv(kFileEnvVar)) {
                file_path = path_env;
            }
        }

        if (file_path.empty()) {
            file_path = "vcuda-hook.log";
        }

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path, true);
        return std::make_shared<spdlog::logger>(kLoggerName, std::move(sink));
    }

    // Unrecognized sink type -> default to stderr.
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    return std::make_shared<spdlog::logger>(kLoggerName, std::move(sink));
}

} // namespace util
