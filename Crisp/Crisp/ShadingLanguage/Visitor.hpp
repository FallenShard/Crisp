#pragma once

namespace crisp::sl
{
    struct Literal;
    struct ArraySpecifier;
    struct TypeSpecifier;
    struct Variable;
    struct TypeQualifier;
    struct SubroutineQualifier;
    struct LayoutQualifier;
    struct FullType;
    struct UnaryExpr;
    struct BinaryExpr;
    struct ConditionalExpr;
    struct ListExpr;
    struct GroupingExpr;
    struct StructSpecifier;
    struct ParameterDeclaration;
    struct FunctionPrototype;

    struct ExprStatement;
    struct PrecisionDeclaration;
    struct FunctionDefinition;
    struct StatementBlock;
    struct StructFieldSpecifier;
    struct JumpStatement;
    struct LoopStatement;
    struct ForStatement;
    struct CaseLabel;
    struct SwitchStatement;
    struct IfStatement;
    struct StructFieldDeclaration;
    struct InitDeclaratorList;
    struct BlockDeclaration;

    struct Visitor
    {
        virtual void visit(const Literal& lit)                     = 0;
        virtual void visit(const ArraySpecifier& arr)              = 0;
        virtual void visit(const TypeSpecifier& typeSpec)          = 0;
        virtual void visit(const Variable& var)                    = 0;
        virtual void visit(const TypeQualifier& typeQual)          = 0;
        virtual void visit(const SubroutineQualifier& subroutine)  = 0;
        virtual void visit(const LayoutQualifier& layout)          = 0;
        virtual void visit(const FullType& fullType)               = 0;
        virtual void visit(const UnaryExpr& unary)                 = 0;
        virtual void visit(const BinaryExpr& binary)               = 0;
        virtual void visit(const ConditionalExpr& cond)            = 0;
        virtual void visit(const ListExpr& list)                   = 0;
        virtual void visit(const GroupingExpr& group)              = 0;
        virtual void visit(const StructSpecifier& structSpec)      = 0;
        virtual void visit(const ParameterDeclaration& param)      = 0;
        virtual void visit(const FunctionPrototype& funcPrototype) = 0;

        virtual void visit(const ExprStatement& statement)        = 0;
        virtual void visit(const PrecisionDeclaration& statement) = 0;
        virtual void visit(const FunctionDefinition& statement)   = 0;
        virtual void visit(const StatementBlock& block)           = 0;
        virtual void visit(const StructFieldSpecifier& s)         = 0;
        virtual void visit(const JumpStatement& j)                = 0;
        virtual void visit(const LoopStatement& l)                = 0;
        virtual void visit(const ForStatement& f)                 = 0;
        virtual void visit(const CaseLabel& c)                    = 0;
        virtual void visit(const SwitchStatement& s)              = 0;
        virtual void visit(const IfStatement& i)                  = 0;
        virtual void visit(const StructFieldDeclaration& sfd)     = 0;
        virtual void visit(const InitDeclaratorList& idl)         = 0;
        virtual void visit(const BlockDeclaration& block)         = 0;
    };
}