#pragma once

#include <Crisp/Common/Format.hpp>
#include <Crisp/Common/HashMap.hpp>
#include <Crisp/Common/Result.hpp>
#include <Crisp/Utils/StringUtils.hpp>

#include <any>
#include <charconv>
#include <filesystem>
#include <functional>

namespace crisp
{
template <typename T>
concept IsArithmetic = std::integral<T> || std::floating_point<T>;

template <typename T>
concept StringLike = std::same_as<T, std::string> || std::same_as<T, std::filesystem::path>;

template <typename T>
concept IsCommandLineArgumentType = std::same_as<T, bool> || IsArithmetic<T> || StringLike<T>;

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

    template <IsCommandLineArgumentType T>
    void addOption(const std::string_view name, T& variable, bool isRequired = false)
    {
        const std::string nameStr(name);
        m_argMap.emplace(nameStr, Argument{nameStr, nullptr, isRequired});
        if constexpr (std::same_as<T, bool>)
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
        else if constexpr (IsArithmetic<T>)
        {
            m_argMap.at(nameStr).parser = [&variable](const std::string_view input)
            {
                std::from_chars(input.data(), input.data() + input.size(), variable);
            };
        }
        else if constexpr (StringLike<T>)
        {
            m_argMap.at(nameStr).parser = [&variable](const std::string_view input)
            {
                variable = T(input);
            };
        }
    }

    [[nodiscard]] Result<> parse(int argc, char** argv);
    [[nodiscard]] Result<> parse(const std::string_view commandLine);
    [[nodiscard]] Result<> parse(const std::vector<std::string_view>& tokens);

private:
    robin_hood::unordered_flat_map<std::string, Argument> m_argMap{};
};
} // namespace crisp
