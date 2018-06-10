#pragma once

#include "ConsoleUtils.hpp"
#include <iostream>

namespace crisp
{
    enum class Severity
    {
        Info,
        Debug,
        Warning,
        Error,
        Fatal
    };

    namespace internal
    {
        template <Severity sev> struct SeverityConsoleColor { };
        template <> struct SeverityConsoleColor<Severity::Info>    { static constexpr ConsoleColor value = ConsoleColor::LightGreen; };
        template <> struct SeverityConsoleColor<Severity::Debug>   { static constexpr ConsoleColor value = ConsoleColor::LightCyan; };
        template <> struct SeverityConsoleColor<Severity::Warning> { static constexpr ConsoleColor value = ConsoleColor::Yellow; };
        template <> struct SeverityConsoleColor<Severity::Error>   { static constexpr ConsoleColor value = ConsoleColor::LightRed; };
        template <> struct SeverityConsoleColor<Severity::Fatal>   { static constexpr ConsoleColor value = ConsoleColor::LightMagenta; };
    }

    template <Severity severity, typename... Args>
    inline static void log(Args&&... args)
    {
        ConsoleColorizer colorizer(internal::SeverityConsoleColor<severity>::value);
        (std::cout << ... << args) << '\n';
    }

    template <typename... Args>
    inline static void logInfo(Args&&... args)
    {
        log<Severity::Info>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline static void logDebug(Args&&... args)
    {
        log<Severity::Debug>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline static void logWarning(Args&&... args)
    {
        log<Severity::Warning>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline static void logError(Args&&... args)
    {
        log<Severity::Error>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline static void logFatal(Args&&... args)
    {
        log<Severity::Fatal>(std::forward<Args>(args)...);
    }
}