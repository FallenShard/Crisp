#pragma once

#include "ConsoleUtils.hpp"

#pragma warning(push)
#pragma warning(disable:26451)
#pragma warning(disable:26495)
#pragma warning(disable:6387)
#define FMT_HEADER_ONLY
#include "fmt/format.h"
#pragma warning(pop)

namespace crisp
{
    template <typename StringType, typename... Args>
    inline static void logInfo(StringType&& string, Args&&... args)
    {
        fmt::print(std::forward<StringType>(string), std::forward<Args>(args)...);
    }

    template <typename StringType, typename... Args>
    inline static void logDebug(StringType&& string, Args&&... args)
    {
        ConsoleColorizer colorizer(ConsoleColor::LightCyan);
        fmt::print(std::forward<StringType>(string), std::forward<Args>(args)...);
    }

    template <typename StringType, typename... Args>
    inline static void logWarning(StringType&& string, Args&&... args)
    {
        ConsoleColorizer colorizer(ConsoleColor::Yellow);
        fmt::print(std::forward<StringType>(string), std::forward<Args>(args)...);
    }

    template <typename StringType, typename... Args>
    inline static void logError(StringType&& string, Args&&... args)
    {
        ConsoleColorizer colorizer(ConsoleColor::LightRed);
        fmt::print(std::forward<StringType>(string), std::forward<Args>(args)...);
    }

    template <typename StringType, typename... Args>
    inline static void logFatal(StringType&& string, Args&&... args)
    {
        ConsoleColorizer colorizer(ConsoleColor::LightMagenta);
        fmt::print(std::forward<StringType>(string), std::forward<Args>(args)...);
        std::exit(-1);
    }
}