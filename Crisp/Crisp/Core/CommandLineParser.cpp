#include <Crisp/Core/CommandLineParser.hpp>

#include <span>

namespace crisp
{
Result<> CommandLineParser::parse(const int32_t argc, char** argv)
{
    const std::span<char*> args(argv, static_cast<size_t>(argc));

    std::vector<std::string_view> commandLineArgs;
    commandLineArgs.reserve(argc);
    for (const auto& arg : args)
    {
        commandLineArgs.emplace_back(arg);
    }

    return parse(commandLineArgs);
}

Result<> CommandLineParser::parse(const std::string_view commandLine)
{
    return parse(tokenizeIntoViews(commandLine, " "));
}

Result<> CommandLineParser::parse(const std::vector<std::string_view>& tokens)
{
    std::size_t i = 1;
    while (i < tokens.size())
    {
        // Found a case of 'variable=value'
        if (const std::size_t pos = tokens[i].find('='); pos != std::string_view::npos)
        {
            const std::string_view name = tokens[i].substr(0, pos).substr(tokens[i].find_first_not_of('-'));
            if (const auto iter = m_argMap.find(std::string(name)); iter != m_argMap.end())
            {
                iter->second.parser(tokens[i].substr(pos + 1));
                iter->second.parsed = true;
            }

            ++i;
        }
        else // Else apply the rule 'variable value'
        {
            if (i + 1 >= tokens.size())
            {
                break;
            }

            const std::string_view name = tokens[i].substr(tokens[i].find_first_not_of('-'));
            if (const auto iter = m_argMap.find(std::string(name)); iter != m_argMap.end())
            {
                iter->second.parser(tokens[i + 1]);
                iter->second.parsed = true;
            }

            i += 2;
        }
    }

    for (const auto& arg : m_argMap)
    {
        if (arg.second.required && !arg.second.parsed)
        {
            return resultError("Missing argument: {}", arg.second.name);
        }
    }

    return {};
}
} // namespace crisp