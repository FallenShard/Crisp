#pragma once

#include "Visitor.hpp"
#include "Token.hpp"

#include <memory>
#include <vector>

namespace crisp::sl
{
    struct Expr
    {
        virtual ~Expr() {}

        template <typename T>
        inline T* as()
        {
            return static_cast<T*>(this);
        }

        virtual void accept(Visitor& visitor) = 0;
    };

    using ExprPtr = std::unique_ptr<Expr>;

    struct Statement;
    using StatementPtr = std::unique_ptr<Statement>;

    struct Literal : public Expr
    {
        Literal(TokenType type, std::any value) : type(type), value(value) {}
        TokenType type;
        std::any value;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct ArraySpecifier : public Expr
    {
        ArraySpecifier() : expr(nullptr) {}
        ArraySpecifier(ExprPtr expr) : expr(std::move(expr)) {}
        ExprPtr expr;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct TypeSpecifier : public Expr
    {
        TypeSpecifier(Token typeToken) : type(typeToken) {}
        Token type;
        std::vector<std::unique_ptr<ArraySpecifier>> arraySpecifiers;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct Variable : public Expr
    {
        Variable(Token name) : name(name) {}
        Token name;
        std::vector<std::unique_ptr<ArraySpecifier>> arraySpecifiers;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct TypeQualifier : public Expr
    {
        TypeQualifier(Token qualifier) : qualifier(qualifier) {}

        Token qualifier;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct SubroutineQualifier : public TypeQualifier
    {
        SubroutineQualifier(Token name) : TypeQualifier(name) {}
        SubroutineQualifier(Token name, std::vector<ExprPtr> exprs) : TypeQualifier(name), exprs(std::move(exprs)) {}

        std::vector<ExprPtr> exprs;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct LayoutQualifier : public TypeQualifier
    {
        LayoutQualifier(Token name) : TypeQualifier(name) {}
        LayoutQualifier(Token name, std::vector<ExprPtr> exprs) : TypeQualifier(name), ids(std::move(exprs)) {}

        std::vector<ExprPtr> ids;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct FullType : public Expr
    {
        std::vector<std::unique_ptr<TypeQualifier>> qualifiers;
        std::unique_ptr<TypeSpecifier> specifier;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct UnaryExpr : public Expr
    {
        UnaryExpr(Token op, ExprPtr expr) : op(op), expr(std::move(expr)) {}

        ExprPtr expr;
        Token op;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct BinaryExpr : public Expr
    {
        ExprPtr left;
        ExprPtr right;
        Token op;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct ConditionalExpr : public Expr
    {
        ExprPtr test;
        ExprPtr thenExpr;
        ExprPtr elseExpr;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct ListExpr : public Expr
    {
        std::string name;
        std::vector<ExprPtr> expressions;

        ListExpr(std::string name) : name(name) {}
        ListExpr(std::string name, std::vector<ExprPtr>&& exprs) : name(name), expressions(std::move(exprs)) {}

        template <typename BaseType>
        ListExpr(std::string name, std::vector<BaseType>&& exprs)
            : name(name)
        {
            for (auto& e : exprs)
                expressions.push_back(std::move(e));
        }

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct GroupingExpr : public Expr
    {
        ExprPtr expr;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct StructSpecifier : public TypeSpecifier
    {
        StructSpecifier(Token typeName) : TypeSpecifier(typeName) {}

        Token structName;
        std::vector<std::unique_ptr<StructFieldDeclaration>> fields;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct ParameterDeclaration : public Expr
    {
        std::unique_ptr<FullType> fullType;
        std::unique_ptr<Variable> var;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct FunctionPrototype : public Expr
    {
        std::unique_ptr<FullType> fullType;
        Token name;
        std::vector<std::unique_ptr<ParameterDeclaration>> params;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };
}