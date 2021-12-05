#pragma once

#include <CrispCore/Format.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace crisp
{
    inline std::shared_ptr<spdlog::logger> createLoggerSt(std::string_view loggerName)
    {
        return spdlog::stdout_color_st(std::string(loggerName));
    }

    inline std::shared_ptr<spdlog::logger> createLoggerMt(std::string_view loggerName)
    {
        return spdlog::stdout_color_mt(std::string(loggerName));
    }
}
