#ifndef UTIL_LOGGER_HPP
#define UTIL_LOGGER_HPP

#include <memory>
#include "spdlog/spdlog.h"

namespace util {

class Logger {
public:
    static void init();

private:
    static spdlog::level::level_enum parseLevel(const char* value);
    static std::shared_ptr<spdlog::logger> buildLogger(const char* sink_value);
};

} // namespace util

#endif // UTIL_LOGGER_HPP
