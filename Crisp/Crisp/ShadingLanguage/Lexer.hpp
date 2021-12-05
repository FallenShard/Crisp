#pragma once

#include <Crisp/ShadingLanguage/Token.hpp>

#include <filesystem>

namespace crisp::sl
{
    class Lexer
    {
    public:
        Lexer(const std::string& source);
        Lexer(std::string&& source);

        std::vector<Token> scanTokens();

    private:
        bool isAtEnd() const;
        void scanToken();
        char peek() const;
        char peekNext() const;
        char advance();
        bool match(char expected);
        bool isDigit(char c) const;
        bool isHexadecimalDigit(char c) const;
        bool isAlpha(char c) const;
        bool isAlphaNumeric(char c) const;

        void addToken(TokenType type);
        void addToken(TokenType type, std::any literal);

        void addString();
        void addNumber();
        void addIdentifier();

        std::string m_source;
        std::vector<Token> m_tokens;

        int m_start;
        int m_current;
        int m_line;
    };
}