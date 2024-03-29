#pragma once

#include <Crisp/Core/Format.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string_view>

namespace crisp {
inline std::shared_ptr<spdlog::logger> createLoggerSt(std::string_view loggerName) {
    return spdlog::stdout_color_st(std::string(loggerName));
}

inline std::shared_ptr<spdlog::logger> createLoggerMt(std::string_view loggerName) {
    return spdlog::stdout_color_mt(std::string(loggerName));
}
} // namespace crisp
