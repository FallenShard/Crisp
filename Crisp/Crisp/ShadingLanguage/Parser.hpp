#pragma once

#include <Crisp/ShadingLanguage/Lexer.hpp>
#include <Crisp/ShadingLanguage/Statements.hpp>

#include <memory>
#include <iostream>
#include <optional>

namespace crisp::sl
{
    class Parser
    {
    public:
        Parser(const std::vector<Token>& tokens);

        std::vector<std::unique_ptr<Statement>> parse();

    private:
        /// <summary>
        /// Checks if the parser has reached the last token in the stream.
        /// </summary>
        /// <returns>Bool if the current token is EndOfFile</returns>
        bool isAtEnd() const;

        /// <summary>
        /// Checks if the parser is currently on the token with the given type.
        /// </summary>
        /// <param name="type">The type to check.</param>
        /// <returns>Bool if the current token is the given type.</returns>
        bool check(TokenType type) const;

        /// <summary>
        /// Returns a copy of the current token.
        /// </summary>
        /// <returns></returns>
        Token peek() const;

        /// <summary>
        /// Returns a copy of the previous token.
        /// </summary>
        /// <returns></returns>
        Token previous() const;

        /// <summary>
        /// Returns a copy of the current token and increments the current token index.
        /// </summary>
        /// <returns></returns>
        Token advance();
        std::optional<Token> consume(TokenType type);
        std::optional<Token> consume(std::initializer_list<TokenType> types);

        bool match(TokenType type);
        bool match(std::initializer_list<TokenType> types);

        ExpressionPtr primary();
        ExpressionPtr postfix();
        ExpressionPtr callArgs(std::string semanticName);

        ExpressionPtr unary();
        ExpressionPtr multiplication();
        ExpressionPtr addition();
        ExpressionPtr shift();
        ExpressionPtr comparison();
        ExpressionPtr equality();
        ExpressionPtr bitwiseAnd();
        ExpressionPtr bitwiseXor();
        ExpressionPtr bitwiseOr();
        ExpressionPtr logicalAnd();
        ExpressionPtr logicalXor();
        ExpressionPtr logicalOr();
        ExpressionPtr conditional();
        ExpressionPtr assignment();
        ExpressionPtr expression();

        StatementPtr declaration();
        std::unique_ptr<FunctionPrototype> functionPrototype();
        std::unique_ptr<ParameterDeclaration> paramDeclaration();
        std::unique_ptr<InitDeclaratorList> initDeclaratorList();

        std::unique_ptr<FullType> fullySpecifiedType();
        std::unique_ptr<LayoutQualifier> layoutQualifier();
        ExpressionPtr layoutQualifierId();

        std::unique_ptr<TypeQualifier> typeQualifier();
        std::vector<std::unique_ptr<TypeQualifier>> typeQualifierList();
        std::unique_ptr<TypeSpecifier> typeSpecifier();

        std::vector<std::unique_ptr<ArraySpecifier>> arraySpecifiers();

        std::unique_ptr<TypeSpecifier> typeSpecifierNonarray();
        std::unique_ptr<StructFieldDeclaration> structFieldDeclaration();
        ExpressionPtr initializer();

        StatementPtr statement();
        StatementPtr simpleStatement();
        std::unique_ptr<ExprStatement> expressionStatement();
        std::unique_ptr<IfStatement> selectionStatement();
        ExpressionPtr conditionExpr();
        std::unique_ptr<SwitchStatement> switchStatement();
        std::unique_ptr<CaseLabel> caseLabel();
        std::unique_ptr<LoopStatement> iterationStatement();
        std::unique_ptr<JumpStatement> jumpStatement();

        StatementPtr externalDeclaration();
        std::unique_ptr<FunctionDefinition> functionDefinition();

        template <typename ReturnType, typename F>
        inline std::unique_ptr<ReturnType> tryParse(F func)
        {
            int current = m_current;
            auto statement = (this->*func)();

            // Reset pointer if the parsing failed
            if (!statement)
                m_current = current;

            return statement;
        }

        std::vector<Token> m_tokens;

        int m_current;
    };
}