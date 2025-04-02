#pragma once

#include <memory>
#include <string_view>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <Crisp/Core/Format.hpp>

namespace crisp {
inline std::shared_ptr<spdlog::logger> createLoggerSt(std::string_view loggerName) {
    return spdlog::stdout_color_st(std::string(loggerName));
}

inline std::shared_ptr<spdlog::logger> createLoggerMt(std::string_view loggerName) {
    return spdlog::stdout_color_mt(std::string(loggerName));
}

inline void setDefaultSpdlogPattern() {
    spdlog::set_pattern("%^[%T.%e][%t][%n][%l]%$\033[34m[%#]\033[0m: %v");
}
} // namespace crisp

#define CRISP_MAKE_LOGGER_ST(channelName) auto logger = ::crisp::createLoggerSt(channelName);
#define CRISP_MAKE_LOGGER_MT(channelName) auto logger = ::crisp::createLoggerMt(channelName);

#define CRISP_LOGT(...) SPDLOG_LOGGER_TRACE(logger, __VA_ARGS__);
#define CRISP_LOGD(...) SPDLOG_LOGGER_DEBUG(logger, __VA_ARGS__);
#define CRISP_LOGI(...) SPDLOG_LOGGER_INFO(logger, __VA_ARGS__);
#define CRISP_LOGW(...) SPDLOG_LOGGER_WARN(logger, __VA_ARGS__);
#define CRISP_LOGE(...) SPDLOG_LOGGER_ERROR(logger, __VA_ARGS__);
#define CRISP_LOGF(...)                                                                                                \
    {                                                                                                                  \
        SPDLOG_LOGGER_CRITICAL(logger, __VA_ARGS__);                                                                   \
        std::abort();                                                                                                  \
    }