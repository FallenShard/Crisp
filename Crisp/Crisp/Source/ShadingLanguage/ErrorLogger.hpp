#pragma once

#include <CrispCore/Log.hpp>

namespace crisp::sl
{
    class ErrorLogger
    {
    public:
        inline static void error(int line, const std::string& message)
        {
            report(line, "", message);
        }

        template <typename T>
        inline static void report(int line, const std::string& where, T&& message)
        {
            logError("[Line {}] Error {}: {}\n", line, where, message);
        }
    };
}
