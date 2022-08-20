#pragma once

#include <Crisp/Common/Result.hpp>
#include <Crisp/Common/RobinHood.hpp>
#include <Crisp/StringUtils.hpp>

#include <any>
#include <charconv>
#include <filesystem>
#include <functional>

namespace crisp
{
class CommandLineParser
{
public:
    struct Argument
    {
        std::string name;
        std::function<void(const std::string_view)> parser;
        bool required;
        bool parsed;
    };

    template <typename T>
    void addOption(const std::string_view name, T& variable, bool isRequired = false)
    {
        const std::string nameStr(name);
        m_argMap.emplace(nameStr, Argument{nameStr, nullptr, isRequired});
        if constexpr (std::is_same_v<T, bool>)
        {
            m_argMap.at(nameStr).parser = [&variable](const std::string_view input)
            {
                std::string in;
                std::transform(
                    input.begin(),
                    input.end(),
                    std::back_inserter(in),
                    [](unsigned char c)
                    {
                        return static_cast<unsigned char>(std::tolower(c));
                    });

                variable = (in == "true" || in == "on");
            };
        }
        else if constexpr (std::is_arithmetic_v<T>)
        {
            m_argMap.at(nameStr).parser = [&variable](const std::string_view input)
            {
                std::from_chars(input.data(), input.data() + input.size(), variable);
            };
        }
        else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::filesystem::path>)
        {
            m_argMap.at(nameStr).parser = [&variable](const std::string_view input)
            {
                variable = T(input);
            };
        }
        else
        {
            std::exit(-1);
            // static_assert(false, "Invalid type specified during CommandLineParser::addOption");
        }
    }

    [[nodiscard]] Result<> parse(int argc, char** argv);
    [[nodiscard]] Result<> parse(const std::string_view commandLine);
    [[nodiscard]] Result<> parse(const std::vector<std::string_view>& tokens);

private:
    robin_hood::unordered_flat_map<std::string, Argument> m_argMap{};
};
} // namespace crisp
