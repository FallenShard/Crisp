#pragma once

#include <memory>

#include "Lexer.hpp"

#include "Statements.hpp"

#include <iostream>

namespace crisp::sl
{
    class Parser
    {
    public:
        Parser(const std::vector<Token>& tokens);

        std::vector<std::unique_ptr<Statement>> parse();

    private:
        bool isAtEnd() const;
        bool check(TokenType type) const;
        const Token& peek() const;
        const Token& advance();
        const Token& previous() const;
        const Token& consume(TokenType type);
        const Token& consume(std::initializer_list<TokenType> types);

        bool match(TokenType type);
        bool match(std::initializer_list<TokenType> types);

        ExprPtr primary();
        ExprPtr postfix();
        ExprPtr callArgs(std::string semanticName);

        ExprPtr unary();
        ExprPtr multiplication();
        ExprPtr addition();
        ExprPtr shift();
        ExprPtr comparison();
        ExprPtr equality();
        ExprPtr bitwiseAnd();
        ExprPtr bitwiseXor();
        ExprPtr bitwiseOr();
        ExprPtr logicalAnd();
        ExprPtr logicalXor();
        ExprPtr logicalOr();
        ExprPtr conditional();
        ExprPtr assignment();
        ExprPtr expression();

        StatementPtr declaration();
        std::unique_ptr<FunctionPrototype> functionPrototype();
        std::unique_ptr<ParameterDeclaration> paramDeclaration();
        std::unique_ptr<InitDeclaratorList> initDeclaratorList();

        std::unique_ptr<FullType> fullySpecifiedType();
        std::unique_ptr<LayoutQualifier> layoutQualifier();
        ExprPtr layoutQualifierId();

        std::unique_ptr<TypeQualifier> typeQualifier();
        std::vector<std::unique_ptr<TypeQualifier>> typeQualifierList();
        std::unique_ptr<TypeSpecifier> typeSpecifier();

        std::vector<std::unique_ptr<ArraySpecifier>> arraySpecifiers();

        std::unique_ptr<TypeSpecifier> typeSpecifierNonarray();
        std::unique_ptr<StructFieldDeclaration> structFieldDeclaration();
        ExprPtr initializer();

        StatementPtr statement();
        StatementPtr simpleStatement();
        std::unique_ptr<ExprStatement> expressionStatement();
        std::unique_ptr<IfStatement> selectionStatement();
        ExprPtr conditionExpr();
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
            try
            {
                auto statement = (this->*func)();
                return statement;
            }
            catch (std::runtime_error err)
            {
                m_current = current; // Reset the token pointer
                return nullptr;
            }
        }

        std::vector<Token> m_tokens;

        int m_current;
    };
}