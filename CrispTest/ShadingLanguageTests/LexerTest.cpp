#include "gtest/gtest.h"

#include "ShadingLanguage/Lexer.hpp"
#include "ShadingLanguage/Parser.hpp"

using namespace crisp::sl;

namespace
{
    const std::string testSource = ""
        "#version 450 core\n"
        "\n"
        "layout(location = 0) in vec2 position;\n"
        "\n"
        "layout(location = 0) out vec2 texCoord;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    texCoord = position * 0.5f + 0.5f;\n"
        "    gl_Position = vec4(position, 0.0f, 1.0f);\n"
        "}\n";
}

TEST(LexerTest, Basic)
{
    Lexer lexer(testSource);
    auto tokens = lexer.scanTokens();

    ASSERT_TRUE(tokens[0].lexeme == "layout");
    ASSERT_EQ(tokens[0].type, TokenType::Layout);

    ASSERT_TRUE(tokens[1].lexeme == "(");
    ASSERT_EQ(tokens[1].type, TokenType::LeftParen);

    ASSERT_TRUE(tokens[6].lexeme == "in");
    ASSERT_EQ(tokens[6].type, TokenType::In);

    ASSERT_TRUE(tokens.back().lexeme == "");
    ASSERT_EQ(tokens.back().type, TokenType::EndOfFile);

    ASSERT_EQ(tokens.size(), 46);
}

TEST(ParserTest, Basic)
{
    Parser parser(Lexer(testSource).scanTokens());

    auto statements = parser.parse();
    ASSERT_EQ(statements.size(), 3);
}