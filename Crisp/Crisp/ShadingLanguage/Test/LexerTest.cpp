#include "gtest/gtest.h"

#include <Crisp/ShadingLanguage/Lexer.hpp>
#include <Crisp/ShadingLanguage/Parser.hpp>

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

    const std::string testSourceVertexShader = ""
        "#version 450 core\n"
        "\n"
        "layout(location = 0) in vec3 position;\n"
        "layout(location = 1) in vec3 normal;\n"
        "layout(location = 2) in vec2 texCoord;\n"
        "layout(location = 3) in vec4 tangent;\n"
        "\n"
        "layout(location = 0) out vec2 texCoord;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    texCoord = position.xy * 0.5f + 0.5f;\n"
        "    gl_Position = vec4(position, 1.0f);\n"
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

TEST(ParserTest, Input)
{
    Parser parser(Lexer(testSourceVertexShader).scanTokens());

    auto statements = parser.parse();
    ASSERT_EQ(statements.size(), 6);

    std::vector<std::tuple<int, std::string, std::string>> attribInputs;
    for (uint32_t i = 0; i < statements.size(); ++i)
    {
        auto parseLayoutQualifier = [&](const LayoutQualifier& layoutQualifier)
        {
            for (auto& id : layoutQualifier.ids)
            {
                if (auto bin = dynamic_cast<BinaryExpr*>(id.get()))
                {
                    auto* left = dynamic_cast<Variable*>(bin->left.get());
                    auto* right = dynamic_cast<Literal*>(bin->right.get());
                    if (left && right)
                    {
                        if (left->name.lexeme == "location")
                        {
                            int attribIdx = std::any_cast<int>(right->value);
                            return attribIdx;
                        }
                    }
                }
            }

            return -1;
        };

        if (auto initDeclList = dynamic_cast<InitDeclaratorList*>(statements[i].get()))
        {
            bool isInput = false;
            int idx;
            std::string name;
            for (auto& qualifier : initDeclList->fullType->qualifiers)
            {
                if (qualifier->qualifier.type == TokenType::Layout)
                {
                    auto layoutQualifier = dynamic_cast<LayoutQualifier*>(qualifier.get());
                    idx = parseLayoutQualifier(*layoutQualifier);

                    name = initDeclList->vars[0]->name.lexeme;

                }
                else if (qualifier->qualifier.type == TokenType::In)
                {
                    isInput = true;
                }
            }

            if (isInput)
            {
                attribInputs.emplace_back(idx, name, initDeclList->fullType->specifier->type.lexeme);
            }
        }
    }

    ASSERT_EQ(attribInputs.size(), 4);

    std::sort(attribInputs.begin(), attribInputs.end());
    ASSERT_EQ(std::get<1>(attribInputs[0]), "position");
    ASSERT_EQ(std::get<2>(attribInputs[3]), "vec4");
}