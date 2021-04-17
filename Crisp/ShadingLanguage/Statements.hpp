#pragma once

#include "Visitor.hpp"
#include "Expressions.hpp"

namespace crisp::sl
{
    struct Statement
    {
        virtual ~Statement() {}

        template <typename T>
        inline T* as()
        {
            return static_cast<T*>(this);
        }

        virtual void accept(Visitor& visitor) = 0;
    };

    using StatementPtr = std::unique_ptr<Statement>;

    struct ExprStatement : public Statement
    {
        ExprStatement(ExpressionPtr expr) : expr(std::move(expr)) {}
        ExpressionPtr expr;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct PrecisionDeclaration : public Statement
    {
        PrecisionDeclaration(Token q, std::unique_ptr<TypeSpecifier> typeSpec)
            : qualifier(q), type(std::move(typeSpec)) {}

        Token qualifier;
        std::unique_ptr<TypeSpecifier> type;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct FunctionDefinition : public Statement
    {
        FunctionDefinition() {}
        FunctionDefinition(std::unique_ptr<FunctionPrototype> prototype, std::unique_ptr<StatementBlock> scope)
            : prototype(std::move(prototype))
            , scope(std::move(scope))
        {}

        std::unique_ptr<FunctionPrototype> prototype;
        std::unique_ptr<StatementBlock> scope;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct StatementBlock : public Statement
    {
        std::string name;
        std::vector<StatementPtr> statements;

        StatementBlock(std::string name) : name(name) {}

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct JumpStatement : public Statement
    {
        Token jump;
        ExpressionPtr result;

        JumpStatement(Token name) : jump(name) {}

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct LoopStatement : public Statement
    {
        Token loop;
        ExpressionPtr condition;
        StatementPtr body;

        LoopStatement(Token name) : loop(name) {}

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct ForStatement : public LoopStatement
    {
        StatementPtr init;
        ExpressionPtr increment;

        ForStatement(Token name) : LoopStatement(name) {}

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct CaseLabel : public Statement
    {
        Token token;
        ExpressionPtr condition;

        CaseLabel(Token name) : token(name) {}

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct SwitchStatement : public Statement
    {
        Token token;
        ExpressionPtr condition;
        std::vector<StatementPtr> body;

        SwitchStatement(Token name) : token(name) {}

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct IfStatement : public Statement
    {
        Token token;
        ExpressionPtr condition;
        StatementPtr thenBranch;
        StatementPtr elseBranch;

        IfStatement(Token name) : token(name) {}

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct StructFieldDeclaration : public Statement
    {
        std::unique_ptr<FullType> fullType;
        std::unique_ptr<Variable> variable;
        std::vector<std::unique_ptr<Variable>> varList;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct InitDeclaratorList : public Statement
    {
        std::unique_ptr<FullType> fullType;
        std::vector<std::unique_ptr<Variable>> vars;
        std::vector<ExpressionPtr> initializers;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct BlockDeclaration : public Statement
    {
        BlockDeclaration(Token name) : name(name) {}

        std::vector<std::unique_ptr<TypeQualifier>> qualifiers;
        Token name;
        std::vector<std::unique_ptr<StructFieldDeclaration>> fields;
        std::vector<std::unique_ptr<Variable>> variables;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };
}
