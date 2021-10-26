#pragma once

#include <CrispCore/StringUtils.hpp>

#include <robin_hood/robin_hood.h>

#include <any>
#include <functional>
#include <charconv>

namespace crisp
{
    class CommandLineParser
    {
    public:
        struct Argument
        {
            std::string name;
            std::any value;
            std::function<std::any(const std::string_view)> parser;
        };

        template <typename T>
        void addOption(const std::string_view name, const T defaultValue)
        {
            const std::string nameStr(name);
            m_argMap.emplace(nameStr, Argument{ nameStr, defaultValue });
            if constexpr (std::is_arithmetic_v<T>)
            {
                m_argMap.at(nameStr).parser = [](const std::string_view input) {
                    T v;
                    std::from_chars(input.data(), input.data() + input.size(), v);
                    return v;
                };
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                m_argMap.at(nameStr).parser = [](const std::string_view input) {
                    return std::string(input);
                };
            }
            else
            {
                static_assert(false, "Invalid type specified during CommandLineParser::addOption");
            }
        }

        void parse(int argc, char** argv);
        void parse(const std::string_view commandLine);
        void parse(const std::vector<std::string_view>& tokens);

        template <typename T>
        T get(const std::string_view name) const
        {
            return std::any_cast<T>(m_argMap.at(std::string(name)).value);
        }

    private:
        robin_hood::unordered_map<std::string, Argument> m_argMap;

    };
}