#include <CrispCore/CommandLineParser.hpp>
#include <span>

namespace crisp
{
    void CommandLineParser::parse(int argc, char** argv)
    {
        const std::span<char*> args(argv, static_cast<size_t>(argc));

        std::vector<std::string_view> commandLineArgs;
        commandLineArgs.reserve(argc);
        for (const auto& arg : args)
        {
            commandLineArgs.emplace_back(arg);
        }

        parse(commandLineArgs);
    }

    void CommandLineParser::parse(const std::string_view commandLine)
    {
        const std::vector<std::string_view> tokens = tokenizeIntoViews(commandLine, " ");
        parse(tokens);
    }

    void CommandLineParser::parse(const std::vector<std::string_view>& tokens)
    {
        std::size_t i = 1;
        while (i < tokens.size())
        {
            // Found a case of 'variable=value'
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
            else  // Else apply the rule 'variable value'
            {
                if (i + 1 >= tokens.size())
                {
                    break;
                }

                const auto iter = m_argMap.find(std::string(tokens[i]));
                if (iter != m_argMap.end())
                {
                    iter->second.value = iter->second.parser(tokens[i + 1]);
                }

                i += 2;
            }
        }
    }
}  // namespace crisp