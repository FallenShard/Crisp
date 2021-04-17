#pragma once

#include "Visitor.hpp"
#include "Token.hpp"

#include <memory>
#include <vector>

namespace crisp::sl
{
    struct Expression
    {
        virtual ~Expression() {}

        template <typename T>
        inline T* as()
        {
            return static_cast<T*>(this);
        }

        virtual void accept(Visitor& visitor) = 0;
    };
    using ExpressionPtr = std::unique_ptr<Expression>;

    struct Literal : public Expression
    {
        Literal(TokenType type, std::any value) : type(type), value(value) {}
        TokenType type;
        std::any value;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct ArraySpecifier : public Expression
    {
        ArraySpecifier() : expr(nullptr) {}
        ArraySpecifier(ExpressionPtr expr) : expr(std::move(expr)) {}
        ExpressionPtr expr;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct TypeSpecifier : public Expression
    {
        TypeSpecifier(Token typeToken) : type(typeToken) {}
        Token type;
        std::vector<std::unique_ptr<ArraySpecifier>> arraySpecifiers;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct Variable : public Expression
    {
        Variable(Token name) : name(name) {}
        Token name;
        std::vector<std::unique_ptr<ArraySpecifier>> arraySpecifiers;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct TypeQualifier : public Expression
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
        SubroutineQualifier(Token name, std::vector<ExpressionPtr>&& exprs) : TypeQualifier(name), exprs(std::move(exprs)) {}

        std::vector<ExpressionPtr> exprs;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct LayoutQualifier : public TypeQualifier
    {
        LayoutQualifier(Token name) : TypeQualifier(name) {}
        LayoutQualifier(Token name, std::vector<ExpressionPtr>&& exprs) : TypeQualifier(name), ids(std::move(exprs)) {}

        std::vector<ExpressionPtr> ids;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct FullType : public Expression
    {
        std::vector<std::unique_ptr<TypeQualifier>> qualifiers;
        std::unique_ptr<TypeSpecifier> specifier;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct UnaryExpr : public Expression
    {
        UnaryExpr(Token op, ExpressionPtr expr) : op(op), expr(std::move(expr)) {}

        ExpressionPtr expr;
        Token op;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct BinaryExpr : public Expression
    {
        ExpressionPtr left;
        ExpressionPtr right;
        Token op;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct ConditionalExpr : public Expression
    {
        ExpressionPtr test;
        ExpressionPtr thenExpr;
        ExpressionPtr elseExpr;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct ListExpr : public Expression
    {
        std::string name;
        std::vector<ExpressionPtr> expressions;

        ListExpr(std::string name) : name(name) {}
        ListExpr(std::string name, std::vector<ExpressionPtr>&& exprs) : name(name), expressions(std::move(exprs)) {}

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

    struct GroupingExpr : public Expression
    {
        ExpressionPtr expr;

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

    struct ParameterDeclaration : public Expression
    {
        std::unique_ptr<FullType> fullType;
        std::unique_ptr<Variable> var;

        virtual void accept(Visitor& visitor) override
        {
            visitor.visit(*this);
        }
    };

    struct FunctionPrototype : public Expression
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
