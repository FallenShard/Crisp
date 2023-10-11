#pragma once

#include <Crisp/ShaderUtils/Token.hpp>

#include <filesystem>

namespace crisp::sl {
class Lexer {
public:
    explicit Lexer(const std::string& source);
    explicit Lexer(std::string&& source);

    std::vector<Token> scanTokens();

private:
    bool isAtEnd() const;
    void scanToken();
    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);
    static bool isDigit(char c);
    static bool isHexadecimalDigit(char c);
    static bool isAlpha(char c);
    static bool isAlphaNumeric(char c);

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
} // namespace crisp::sl