#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <functional>
#include <optional>
#include <system_error>
#include <utility>

#include <Crisp/Core/Format.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/Result.hpp>
#include <Crisp/Utils/StringUtils.hpp>

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
        std::function<Result<>(std::string_view)> parser;
        bool required;
        bool parsed{false};
    };

    template <CommandLineArgumentType T>
    void addOption(const std::string_view name, T& variable, bool isRequired = false) {
        addOptionParser(
            name,
            [name = std::string(name), &variable](const std::string_view input) {
                return parseValue(name, input, variable);
            },
            isRequired);
    }

    template <CommandLineArgumentType T>
    void addOption(const std::string_view name, std::optional<T>& variable, bool isRequired = false) {
        addOptionParser(
            name,
            [name = std::string(name), &variable](const std::string_view input) -> Result<> {
                T parsedValue{};
                CRISP_TRY(parseValue(name, input, parsedValue));
                variable = std::move(parsedValue);
                return kResultSuccess;
            },
            isRequired);
    }

    Result<> parse(int argc, char** argv);
    Result<> parse(std::string_view commandLine);
    Result<> parse(const std::vector<std::string_view>& tokens);

private:
    template <CommandLineArgumentType T>
    static Result<> parseValue(const std::string_view name, const std::string_view input, T& variable) {
        if constexpr (std::same_as<T, bool>) {
            std::string normalized;
            normalized.reserve(input.size());
            std::transform(input.begin(), input.end(), std::back_inserter(normalized), [](const unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            if (normalized == "true" || normalized == "on" || normalized == "1") {
                variable = true;
            } else if (normalized == "false" || normalized == "off" || normalized == "0") {
                variable = false;
            } else {
                return resultError("Invalid value for option '{}': expected a boolean, got '{}'", name, input);
            }
        } else if constexpr (ArithmeticType<T>) {
            const auto* end = input.data() + input.size(); // NOLINT
            T parsedValue{};
            const auto [ptr, error] = std::from_chars(input.data(), end, parsedValue);
            if (error != std::errc{} || ptr != end) {
                return resultError("Invalid numeric value for option '{}': {}", name, input);
            }
            variable = parsedValue;
        } else if constexpr (StringLike<T>) {
            variable = T(input);
        }

        return kResultSuccess;
    }

    void addOptionParser(std::string_view name, std::function<Result<>(std::string_view)> parser, bool isRequired);

    FlatHashMap<std::string, Argument> m_argMap;
};
} // namespace crisp
