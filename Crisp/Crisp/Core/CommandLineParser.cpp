#include <Crisp/Core/CommandLineParser.hpp>

#include <span>

namespace crisp {
namespace {
std::string_view removeOptionPrefix(const std::string_view token) {
    const auto firstCharacter = token.find_first_not_of('-');
    return firstCharacter == std::string_view::npos ? std::string_view{} : token.substr(firstCharacter);
}
} // namespace

void CommandLineParser::addOptionParser(
    const std::string_view name, std::function<Result<>(std::string_view)> parser, const bool isRequired) {
    const std::string nameString(name);
    Argument argument{.name = nameString, .parser = std::move(parser), .required = isRequired};
    if (const auto iter = m_argMap.find(nameString); iter != m_argMap.end()) {
        iter->second = std::move(argument);
    } else {
        m_argMap.emplace(nameString, std::move(argument));
    }
}

Result<> CommandLineParser::parse(const int32_t argc, char** argv) {
    const std::span<char*> args(argv, static_cast<size_t>(argc));

    std::vector<std::string_view> commandLineArgs;
    commandLineArgs.reserve(argc);
    for (const auto& arg : args) {
        commandLineArgs.emplace_back(arg);
    }

    return parse(commandLineArgs);
}

Result<> CommandLineParser::parse(const std::string_view commandLine) {
    return parse(tokenizeIntoViews(commandLine, " "));
}

Result<> CommandLineParser::parse(const std::vector<std::string_view>& tokens) {
    for (auto& entry : m_argMap) {
        entry.second.parsed = false;
    }

    // The first token is the executable name, so we skip it.
    std::size_t i = 1;
    while (i < tokens.size()) {
        const auto token = tokens[i];
        // Found a case of 'variable=value'
        if (const std::size_t pos = token.find('='); pos != std::string_view::npos) {
            const std::string_view name = removeOptionPrefix(token.substr(0, pos));
            const auto iter = m_argMap.find(std::string(name));
            if (iter == m_argMap.end()) {
                return resultError("Unknown argument: {}", name);
            }
            if (auto result = iter->second.parser(token.substr(pos + 1)); !result.isValid()) {
                return result;
            }
            iter->second.parsed = true;

            ++i;
        } else {
            // Else apply the rule 'variable value'
            const std::string_view name = removeOptionPrefix(token);
            const auto iter = m_argMap.find(std::string(name));
            if (iter == m_argMap.end()) {
                return resultError("Unknown argument: {}", name);
            }
            if (i + 1 >= tokens.size()) {
                return resultError("Missing value for argument: {}", name);
            }
            if (tokens[i + 1].starts_with("--")) {
                return resultError("Missing value for argument: {}", name);
            }

            if (auto result = iter->second.parser(tokens[i + 1]); !result.isValid()) {
                return result;
            }
            iter->second.parsed = true;

            i += 2;
        }
    }

    for (const auto& arg : m_argMap) {
        if (arg.second.required && !arg.second.parsed) {
            return resultError("Missing argument: {}", arg.second.name);
        }
    }

    return {};
}
} // namespace crisp
