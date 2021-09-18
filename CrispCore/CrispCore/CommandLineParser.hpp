#pragma once

#include "CrispCore/StringUtils.hpp"

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

        void parse(const std::string_view commandLine)
        {
            const std::vector<std::string_view> tokens = tokenizeIntoViews(commandLine, " ");
            parse(tokens);
        }

        void parse(const std::vector<std::string_view>& tokens)
        {
            std::size_t i = 1;
            while (i < tokens.size())
            {
                // Found a case of variable=value
                if (const std::size_t pos = tokens[i].find('='); pos != std::string_view::npos)
                {
                    const std::string_view name = tokens[i].substr(0, pos);
                    const auto iter = m_argMap.find(std::string(name));
                    if (iter != m_argMap.end())
                    {
                        iter->second.value = iter->second.parser(tokens[i].substr(pos + 1));
                    }

                    ++i;
                }
                else // Apply the rule variable value
                {
                    if (i + 1 >= tokens.size())
                        break;

                    const auto iter = m_argMap.find(std::string(tokens[i]));
                    if (iter != m_argMap.end())
                    {
                        iter->second.value = iter->second.parser(tokens[i + 1]);
                    }

                    i += 2;
                }
            }
        }

        template <typename T>
        T get(const std::string_view name) const
        {
            return std::any_cast<T>(m_argMap.at(std::string(name)).value);
        }

    private:
        robin_hood::unordered_map<std::string, Argument> m_argMap;

    };
}