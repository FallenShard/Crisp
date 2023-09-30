#pragma once

#include <Crisp/Core/Format.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/Result.hpp>
#include <Crisp/Utils/StringUtils.hpp>

#include <any>
#include <charconv>
#include <filesystem>
#include <functional>

namespace crisp {
template <typename T>
concept ArithmeticType = std::integral<T> || std::floating_point<T>;

template <typename T>
concept StringLike = std::same_as<T, std::string> || std::same_as<T, std::filesystem::path>;

template <typename T>
concept CommandLineArgumentType = std::same_as<T, bool> || ArithmeticType<T> || StringLike<T>;

class CommandLineParser {
public:
    struct Argument {
        std::string name;
        std::function<void(const std::string_view)> parser;
        bool required;
        bool parsed{false};
    };

    template <CommandLineArgumentType T>
    void addOption(const std::string_view name, T& variable, bool isRequired = false) {
        const std::string nameStr(name);
        m_argMap.emplace(nameStr, Argument{nameStr, nullptr, isRequired});
        if constexpr (std::same_as<T, bool>) {
            m_argMap.at(nameStr).parser = [&variable](const std::string_view input) {
                std::string in;
                std::transform(input.begin(), input.end(), std::back_inserter(in), [](unsigned char c) {
                    return static_cast<unsigned char>(std::tolower(c));
                });

                variable = (in == "true" || in == "on");
            };
        } else if constexpr (ArithmeticType<T>) {
            m_argMap.at(nameStr).parser = [&variable](const std::string_view input) {
                std::from_chars(input.data(), input.data() + input.size(), variable);
            };
        } else if constexpr (StringLike<T>) {
            m_argMap.at(nameStr).parser = [&variable](const std::string_view input) { variable = T(input); };
        }
    }

    Result<> parse(int argc, char** argv);
    Result<> parse(std::string_view commandLine);
    Result<> parse(const std::vector<std::string_view>& tokens);

private:
    FlatHashMap<std::string, Argument> m_argMap{};
};
} // namespace crisp
