#pragma once

#include "Visitor.hpp"
#include "Token.hpp"
#include "Statements.hpp"
#include "Expressions.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>

namespace crisp::sl
{
    struct PrintVisitor : public Visitor
    {
        static const int IndentationStep = 2;
        int level = 0;

        std::ofstream outStream;

        PrintVisitor() { outStream = std::ofstream("test.txt"); }

        std::ostream& indented()
        {
            return std::cout << indent();
        }

        std::string indent()
        {
            return std::string(level, ' ');
        }

        virtual void visit(const Literal& lit) override
        {
            std::stringstream str;
            str << "Literal: ";
            if (lit.type == TokenType::IntConstant)
                str << std::any_cast<int>(lit.value);
            else if (lit.type == TokenType::BoolConstant)
                str << std::boolalpha << std::any_cast<bool>(lit.value);
            else if (lit.type == TokenType::FloatConstant)
                str << std::any_cast<float>(lit.value);
            else if (lit.type == TokenType::DoubleConstant)
                str << std::any_cast<double>(lit.value);
            indented() << str.str() << '\n';
        }

        virtual void visit(const ArraySpecifier& arr) override
        {
            if (arr.expr)
                arr.expr->accept(*this);
        }

        virtual void visit(const Variable& var) override
        {
            indented() << "Variable: " << var.name.lexeme << " ";
            if (!var.arraySpecifiers.empty())
            {

                for (auto& e : var.arraySpecifiers)
                {
                    indented() << "[";
                    e->accept(*this);
                    indented() << "]";
                }
            }

            indented() << '\n';
        }

        virtual void visit(const TypeSpecifier& t) override
        {
            indented() << "Type: " << t.type.lexeme << " ";
            if (!t.arraySpecifiers.empty())
            {

                for (auto& e : t.arraySpecifiers)
                {
                    indented() << "[";
                    e->accept(*this);
                    indented() << "]";
                }
            }

            indented() << '\n';
        }

        virtual void visit(const TypeQualifier& q) override
        {
            indented() << "Qualifier: " << q.qualifier.lexeme << '\n';
        }

        virtual void visit(const SubroutineQualifier& q) override
        {
            indented() << "Subroutine: " << q.qualifier.lexeme << '\n';
            level += IndentationStep;
            for (const auto& e : q.exprs)
                e->accept(*this);
            level -= IndentationStep;
        }

        virtual void visit(const LayoutQualifier& q) override
        {
            indented() << "Layout Qualifier: \n";
            level += IndentationStep;
            for (const auto& e : q.ids)
                e->accept(*this);
            level -= IndentationStep;

            if (q.ids.size() >= 2)
            {
                if (auto p = q.ids[0]->as<BinaryExpr>())
                {
                    auto l = p->left->as<Variable>();
                    if (auto r = p->right->as<Literal>(); r && l && l->name.lexeme == "set")
                    {
                        spdlog::debug("Set: {} ", std::any_cast<int>(r->value));
                    }
                }

                if (auto p = q.ids[1]->as<BinaryExpr>())
                {
                    auto l = p->left->as<Variable>();
                    if (auto r = p->right->as<Literal>(); r&& l&& l->name.lexeme == "binding")
                    {
                        spdlog::debug("Binding: {}", std::any_cast<int>(r->value));
                    }
                }
                std::cout << '\n';
            }
        }

        virtual void visit(const FullType& fullType) override
        {
            for (auto& q : fullType.qualifiers)
                q->accept(*this);
            fullType.specifier->accept(*this);
        }

        virtual void visit(const UnaryExpr& expr) override
        {
            indented() << "Unary Operator: " << expr.op.lexeme << std::endl;
            level += IndentationStep;
            expr.expr->accept(*this);
            level -= IndentationStep;
        }

        virtual void visit(const BinaryExpr& expr) override
        {
            indented() << "Binary Operator: " << expr.op.lexeme << std::endl;
            level += IndentationStep;
            expr.left->accept(*this);
            expr.right->accept(*this);
            level -= IndentationStep;
        }

        virtual void visit(const ConditionalExpr& expr) override
        {
            indented() << "Conditional" << std::endl;
            level += IndentationStep;
            expr.test->accept(*this);
            expr.thenExpr->accept(*this);
            expr.elseExpr->accept(*this);
            level -= IndentationStep;
        }

        virtual void visit(const ListExpr& expr) override
        {
            if (!expr.name.empty())
                indented() << expr.name << ":\n";
            level += IndentationStep;
            for (const auto& e : expr.expressions)
                e->accept(*this);
            level -= IndentationStep;
        }

        virtual void visit(const GroupingExpr& expr) override
        {
            indented() << "Grouping (\n";
            expr.expr->accept(*this);
            indented() << ")\n";
        }

        virtual void visit(const StructSpecifier& expr) override
        {
            indented() << "Struct " << expr.structName.lexeme << "\n";
            level += IndentationStep;
            for (auto& f : expr.fields)
                f->accept(*this);
            level -= IndentationStep;
            indented() << "\n";
        }

        virtual void visit(const ParameterDeclaration& expr) override
        {
            indented() << "Param: ";
            expr.fullType->accept(*this);
            expr.var->accept(*this);
        }

        virtual void visit(const FunctionPrototype& expr) override
        {
            indented() << "Function name: " << expr.name.lexeme << '\n';
            level += IndentationStep;

            expr.fullType;
            indented() << "Params: \n";
            for (auto& p : expr.params)
                p->accept(*this);

            level -= IndentationStep;
            indented() << '\n';
        }

        virtual void visit(const ExprStatement& statement) override
        {
            indented() << "Expression Statement\n";
            if (!statement.expr)
                indented() << "Empty\n";
            else
                statement.expr->accept(*this);
        }

        virtual void visit(const PrecisionDeclaration& statement) override
        {
            indented() << "Precision Statement " << statement.qualifier.lexeme << '\n';
            level += IndentationStep;
            statement.type->accept(*this);
            level -= IndentationStep;
        }

        virtual void visit(const FunctionDefinition& statement) override
        {
            statement.prototype->accept(*this);
            level += IndentationStep;
            statement.scope->accept(*this);
            level -= IndentationStep;
        }

        virtual void visit(const StatementBlock& statement) override
        {
            indented() << statement.name << ":\n";
            level += IndentationStep;
            for (const auto& e : statement.statements)
                e->accept(*this);
            level -= IndentationStep;
        }

        virtual void visit(const StructFieldSpecifier& s) override
        {
            indented() << "StructFieldSpecifier" << ":\n";
        }

        virtual void visit(const JumpStatement& j) override
        {
            indented() << "Jump Statement" << j.jump.lexeme << ":\n";
        }

        virtual void visit(const LoopStatement& l) override
        {
            indented() << "Loop Statement" << l.loop.lexeme << ":\n";
        }

        virtual void visit(const ForStatement& f) override
        {
            indented() << "For Statement" << f.loop.lexeme << ":\n";
        }

        virtual void visit(const CaseLabel& c) override
        {
            indented() << "CaseLabel" << ":\n";
        }

        virtual void visit(const SwitchStatement& s) override
        {
            indented() << "SwitchStatement" << ":\n";
        }

        virtual void visit(const IfStatement& i) override
        {
            indented() << "IfStatement" << ":\n";
        }

        virtual void visit(const StructFieldDeclaration& sfd) override
        {
            indented() << "StructFieldDeclaration" << ":\n";
            sfd.fullType->accept(*this);
            sfd.variable->accept(*this);
        }

        virtual void visit(const InitDeclaratorList& idl) override
        {
            indented() << "Init Declarator List \n";
            idl.fullType->accept(*this);
            for (auto& i : idl.initializers)
                i->accept(*this);
            for (auto& v : idl.vars)
                v->accept(*this);
        }

        virtual void visit(const BlockDeclaration& block) override
        {
            indented() << "Block Declaration - " << block.name.lexeme << "\n";
            level += IndentationStep;
            for (auto& q : block.qualifiers)
                q->accept(*this);
            for (auto& f : block.fields)
                f->accept(*this);
            for (auto& v : block.variables)
                v->accept(*this);

            level -= IndentationStep;
        }
    };
}