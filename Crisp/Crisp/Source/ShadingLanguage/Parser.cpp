#include "Parser.hpp"

#include "ErrorLogger.hpp"

namespace
{

}

namespace crisp::sl
{
    Parser::Parser(const std::vector<Token>& tokens)
        : m_tokens(tokens)
        , m_current(0)
    {
    }

    std::vector<std::unique_ptr<Statement>> Parser::parse()
    {
        std::vector<std::unique_ptr<Statement>> statements;
        while (!isAtEnd())
            statements.emplace_back(externalDeclaration());

        return statements;
    }

    bool Parser::isAtEnd() const
    {
        return peek().type == TokenType::EndOfFile;
    }

    bool Parser::check(TokenType type) const
    {
        if (isAtEnd())
            return false;

        return peek().type == type;
    }

    const Token& Parser::peek() const
    {
        return m_tokens[m_current];
    }

    const Token& Parser::advance()
    {
        if (!isAtEnd())
            ++m_current;

        return previous();
    }

    const Token& Parser::previous() const
    {
        return m_tokens[m_current - 1];
    }

    const Token& Parser::consume(TokenType type)
    {
        if (check(type))
            return advance();

        //ErrorLogger::report(peek().line, "Expected type ", type);

        throw std::runtime_error("What the hell?");
    }

    const Token& Parser::consume(std::initializer_list<TokenType> types)
    {
        for (const auto type : types)
            if (check(type))
                return advance();

        throw std::runtime_error("What the hell multi-consume?");
    }

    bool Parser::match(TokenType type)
    {
        if (check(type))
        {
            advance();
            return true;
        }

        return false;
    }

    bool Parser::match(std::initializer_list<TokenType> types)
    {
        for (const auto type : types)
        {
            if (check(type))
            {
                advance();
                return true;
            }
        }

        return false;
    }

    std::unique_ptr<Expr> Parser::primary()
    {
        if (match({ TokenType::BoolConstant,
            TokenType::IntConstant, TokenType::UintConstant,
            TokenType::FloatConstant, TokenType::DoubleConstant,
            TokenType::String }))
            return std::make_unique<Literal>(previous().type, previous().literal);

        if (match(TokenType::Identifier))
            return std::make_unique<Variable>(previous());

        if (match(TokenType::LeftParen))
        {
            auto group = std::make_unique<GroupingExpr>();
            group->expr = expression();
            consume(TokenType::RightParen);
            return group;
        }

        throw std::runtime_error("Unknown primary expression!");
    }

    std::unique_ptr<Expr> Parser::postfix()
    {
        auto list = std::make_unique<ListExpr>("");
        if (auto p = tryParse<Expr>(&Parser::primary))
        {
            list->expressions.push_back(std::move(p));
        }
        else
        {
            list->expressions.push_back(typeSpecifier());
            consume(TokenType::LeftParen);
            list->expressions.push_back(callArgs("Ctor"));
        }

        while (true)
        {
            if (match(TokenType::LeftParen))
            {
                list->expressions.push_back(callArgs("Fun"));
            }
            else if (match(TokenType::Dot))
            {
                consume(TokenType::Identifier);
                list->expressions.push_back(std::make_unique<Variable>(previous()));
            }
            else if (match({ TokenType::PlusPlus, TokenType::MinusMinus }))
            {
                //++, --
            }
            else if (match(TokenType::LeftBracket))
            {
                list->expressions.push_back(expression());
                consume(TokenType::RightBracket);
            }
            else
                break;
        }

        if (list->expressions.size() == 1)
            return std::move(list->expressions.front());
        else
            return list;
    }

    ExprPtr Parser::callArgs(std::string semanticName)
    {
        auto args = std::make_unique<ListExpr>(semanticName);
        match(TokenType::Void);
        if (!match(TokenType::RightParen))
        {
            args->expressions.push_back(assignment());
            while (match(TokenType::Comma))
                args->expressions.push_back(assignment());
            consume(TokenType::RightParen);
        }

        return args;
    }

    std::unique_ptr<Expr> Parser::unary()
    {
        if (match({ TokenType::PlusPlus, TokenType::MinusMinus,
            TokenType::Plus, TokenType::Minus, TokenType::ExclamationMark,
            TokenType::Tilde }))
        {
            return std::make_unique<UnaryExpr>(previous(), unary());
        }

        return postfix();
    }

    std::unique_ptr<Expr> Parser::multiplication()
    {
        std::unique_ptr<Expr> expr = unary();

        while (match({ TokenType::Star, TokenType::Slash, TokenType::Percent }))
        {
            auto binary = std::make_unique<BinaryExpr>();
            binary->left  = std::move(expr);
            binary->op    = previous();
            binary->right = unary();
            expr = std::move(binary);
        }

        return expr;
    }

    std::unique_ptr<Expr> Parser::addition()
    {
        std::unique_ptr<Expr> expr = multiplication();

        while (match({ TokenType::Plus, TokenType::Minus }))
        {
            auto binary = std::make_unique<BinaryExpr>();
            binary->left  = std::move(expr);
            binary->op    = previous();
            binary->right = multiplication();
            expr = std::move(binary);
        }

        return expr;
    }

    std::unique_ptr<Expr> Parser::shift()
    {
        std::unique_ptr<Expr> expr = addition();

        while (match({ TokenType::BitShiftLeft, TokenType::BitShiftRight }))
        {
            auto binary = std::make_unique<BinaryExpr>();
            binary->left  = std::move(expr);
            binary->op    = previous();
            binary->right = addition();
            expr = std::move(binary);
        }

        return expr;
    }


    std::unique_ptr<Expr> Parser::comparison()
    {
        std::unique_ptr<Expr> expr = shift();

        while (match({ TokenType::GreaterThan, TokenType::GreaterThanEqual,
            TokenType::LessThan, TokenType::LessThanEqual }))
        {
            auto binary = std::make_unique<BinaryExpr>();
            binary->left  = std::move(expr);
            binary->op    = previous();
            binary->right = shift();
            expr = std::move(binary);
        }

        return expr;
    }

    std::unique_ptr<Expr> Parser::equality()
    {
        std::unique_ptr<Expr> expr = comparison();

        while (match({ TokenType::NotEqual, TokenType::EqualEqual }))
        {
            auto binary = std::make_unique<BinaryExpr>();
            binary->left  = std::move(expr);
            binary->op    = previous();
            binary->right = comparison();
            expr = std::move(binary);
        }

        return expr;
    }

    std::unique_ptr<Expr> Parser::bitwiseAnd()
    {
        std::unique_ptr<Expr> expr = equality();

        while (match({ TokenType::BitwiseAnd }))
        {
            auto binary = std::make_unique<BinaryExpr>();
            binary->left  = std::move(expr);
            binary->op    = previous();
            binary->right = equality();
            expr = std::move(binary);
        }

        return expr;
    }

    std::unique_ptr<Expr> Parser::bitwiseXor()
    {
        std::unique_ptr<Expr> expr = bitwiseAnd();

        while (match({ TokenType::BitwiseXor }))
        {
            auto binary = std::make_unique<BinaryExpr>();
            binary->left  = std::move(expr);
            binary->op    = previous();
            binary->right = bitwiseAnd();
            expr = std::move(binary);
        }

        return expr;
    }

    std::unique_ptr<Expr> Parser::bitwiseOr()
    {
        std::unique_ptr<Expr> expr = bitwiseXor();

        while (match({ TokenType::BitwiseOr }))
        {
            auto binary = std::make_unique<BinaryExpr>();
            binary->left  = std::move(expr);
            binary->op    = previous();
            binary->right = bitwiseXor();
            expr          = std::move(binary);
        }

        return expr;
    }

    std::unique_ptr<Expr> Parser::logicalAnd()
    {
        std::unique_ptr<Expr> expr = bitwiseOr();

        while (match({ TokenType::LogicalAnd }))
        {
            auto binary = std::make_unique<BinaryExpr>();
            binary->left  = std::move(expr);
            binary->op    = previous();
            binary->right = bitwiseOr();
            expr = std::move(binary);
        }

        return expr;
    }

    std::unique_ptr<Expr> Parser::logicalXor()
    {
        std::unique_ptr<Expr> expr = logicalAnd();

        while (match({ TokenType::LogicalXor }))
        {
            auto binary = std::make_unique<BinaryExpr>();
            binary->left  = std::move(expr);
            binary->op    = previous();
            binary->right = logicalAnd();
            expr = std::move(binary);
        }

        return expr;
    }

    std::unique_ptr<Expr> Parser::logicalOr()
    {
        std::unique_ptr<Expr> expr = logicalXor();

        while (match({ TokenType::LogicalOr }))
        {
            auto binary = std::make_unique<BinaryExpr>();
            binary->left  = std::move(expr);
            binary->op    = previous();
            binary->right = logicalXor();
            expr = std::move(binary);
        }

        return expr;
    }

    std::unique_ptr<Expr> Parser::conditional()
    {
        std::unique_ptr<Expr> expr = logicalOr();

        if (match(TokenType::QuestionMark))
        {
            auto ternary = std::make_unique<ConditionalExpr>();
            ternary->test = std::move(expr);
            ternary->thenExpr = expression();
            consume(TokenType::FullColon);
            ternary->elseExpr = assignment();
            return ternary;
        }

        return expr;
    }

    std::unique_ptr<Expr> Parser::assignment()
    {
        std::unique_ptr<Expr> expr = conditional();

        // TODO: Add the rest of assignment operators
        while (match({ TokenType::Equal,
            TokenType::PlusEqual, TokenType::MinusEqual,
            TokenType::StarEqual, TokenType::SlashEqual }))
        {
            Token op = previous();
            std::unique_ptr<Expr> right = assignment();

            auto binary = std::make_unique<BinaryExpr>();
            binary->left  = std::move(expr);
            binary->op    = op;
            binary->right = std::move(right);
            expr = std::move(binary);

            //auto binary   = std::make_unique<BinaryExpr>();
            //binary->left  = std::move(expr);
            //binary->op    = op;
            //binary->right = std::move(right);
            //expr = std::move(binary);

            // its a bit messed up
            //if (auto ptr = expr.get(); auto v = dynamic_cast<Variable*>(ptr))
            //{
            //    // Found a variable...
            //    // return make_unique<Assignment>
            //}
        }

        return expr;
    }

    std::unique_ptr<Expr> Parser::expression()
    {
        auto listExpr = std::make_unique<ListExpr>("CommaSepExprs");
        listExpr->expressions.push_back(assignment());
        while (match(TokenType::Comma))
            listExpr->expressions.push_back(assignment());

        if (listExpr->expressions.size() == 1)
            return std::move(listExpr->expressions.front());
        else
            return listExpr;
    }

    std::unique_ptr<Statement> Parser::declaration()
    {
        // Precision declaration
        if (match(TokenType::Precision))
        {
            auto qualifier = consume({ TokenType::LowP, TokenType::MediumP, TokenType::HighP });
            auto type = typeSpecifier();
            if (!type)
                throw std::runtime_error("Invalid type in a precision statement.");

            consume(TokenType::Semicolon);
            return std::make_unique<PrecisionDeclaration>(qualifier, std::move(type));
        }

        // is it a func prototype?
        if (auto a = tryParse<Expr>(&Parser::functionPrototype))
        {
            consume(TokenType::Semicolon);
            return std::make_unique<ExprStatement>(std::move(a));
        }

        // Is it the declarator list? (e.g. struct definition)
        if (auto i = tryParse<Statement>(&Parser::initDeclaratorList))
            return i;

        auto typeQualifiers = typeQualifierList();
        if (typeQualifiers.empty())
            return nullptr;

        if (match(TokenType::Semicolon))
        {
            // Kind of a null statement, just lists type qualifiers
            return std::make_unique<ExprStatement>(std::make_unique<ListExpr>("typeQualifiers", std::move(typeQualifiers)));
        }
        else if (match(TokenType::Identifier))
        {
            // Uniform block and similar stuff
            auto id = previous();
            if (match(TokenType::LeftBrace))
            {
                auto block = std::make_unique<BlockDeclaration>(id);
                block->qualifiers = std::move(typeQualifiers);
                block->fields.push_back(structFieldDeclaration());
                while (!match(TokenType::RightBrace))
                    block->fields.push_back(structFieldDeclaration());

                if (match(TokenType::Identifier))
                {
                    auto var = std::make_unique<Variable>(previous());
                    var->arraySpecifiers = arraySpecifiers();
                    block->variables.push_back(std::move(var));
                }

                consume(TokenType::Semicolon);
                return block;
            }

            // TODO: Global variable with a list of identifiers? But where's the type
            std::vector<Token> ids = { id };
            while (match(TokenType::Comma))
                ids.push_back(consume(TokenType::Identifier));
            consume(TokenType::Semicolon);

            // Nullify it for now
            return std::make_unique<ExprStatement>(std::make_unique<ListExpr>("typeQualifiers", std::move(typeQualifiers)));
        }

        return nullptr;
    }

    std::unique_ptr<FunctionPrototype> Parser::functionPrototype()
    {
        auto fun = std::make_unique<FunctionPrototype>();
        fun->fullType = fullySpecifiedType();
        fun->name = consume(TokenType::Identifier);
        consume(TokenType::LeftParen);
        if (!match(TokenType::RightParen))
        {
            fun->params.push_back(paramDeclaration());
            while (match(TokenType::Comma))
                fun->params.push_back(paramDeclaration());

            consume(TokenType::RightParen);
        }

        return fun;
    }

    std::unique_ptr<ParameterDeclaration> Parser::paramDeclaration()
    {
        auto expr = std::make_unique<ParameterDeclaration>();
        expr->fullType = fullySpecifiedType();
        if (match(TokenType::Identifier))
        {
            expr->var = std::make_unique<Variable>(previous());
            expr->var->arraySpecifiers = arraySpecifiers();
        }

        return expr;
    }

    std::unique_ptr<InitDeclaratorList> Parser::initDeclaratorList()
    {
        auto stmt = std::make_unique<InitDeclaratorList>();
        stmt->fullType = fullySpecifiedType();
        if (match(TokenType::Identifier))
        {
            auto var = std::make_unique<Variable>(previous());
            var->arraySpecifiers = arraySpecifiers();
            stmt->vars.push_back(std::move(var));

            if (match(TokenType::Equal))
                stmt->initializers.push_back(initializer());
        }

        while (match(TokenType::Comma))
        {
            auto var = std::make_unique<Variable>(consume(TokenType::Identifier));
            var->arraySpecifiers = arraySpecifiers();
            stmt->vars.push_back(std::move(var));

            if (match(TokenType::Equal))
                stmt->initializers.push_back(initializer());
        }

        consume(TokenType::Semicolon);
        return stmt;
    }

    std::unique_ptr<FullType> Parser::fullySpecifiedType()
    {
        auto expr        = std::make_unique<FullType>();
        expr->qualifiers = typeQualifierList();
        expr->specifier  = typeSpecifier();
        return expr;
    }

    std::unique_ptr<LayoutQualifier> Parser::layoutQualifier()
    {
        if (!match(TokenType::Layout))
            return nullptr;

        auto layout = std::make_unique<LayoutQualifier>(previous());
        consume(TokenType::LeftParen);

        layout->ids.push_back(layoutQualifierId());
        while (match(TokenType::Comma))
            layout->ids.push_back(layoutQualifierId());

        consume(TokenType::RightParen);
        return layout;
    }

    std::unique_ptr<Expr> Parser::layoutQualifierId()
    {
        if (match(TokenType::Shared))
            return std::make_unique<Variable>(previous());

        ExprPtr id = std::make_unique<Variable>(consume(TokenType::Identifier));
        if (match(TokenType::Equal))
        {
            auto binaryExpr   = std::make_unique<BinaryExpr>();
            binaryExpr->left  = std::move(id);
            binaryExpr->op    = previous();
            binaryExpr->right = conditional();
            id = std::move(binaryExpr);
        }

        return id;
    }

    std::unique_ptr<TypeQualifier> Parser::typeQualifier()
    {
        // Storage Qualifier
        if (match({ TokenType::Const,
            TokenType::In,
            TokenType::Out,
            TokenType::InOut,
            TokenType::Centroid,
            TokenType::Patch,
            TokenType::Sample,
            TokenType::Uniform,
            TokenType::Buffer,
            TokenType::Shared,
            TokenType::Coherent,
            TokenType::Volatile,
            TokenType::Restrict,
            TokenType::ReadOnly,
            TokenType::WriteOnly }))
            return std::make_unique<TypeQualifier>(previous());

        // Subroutine (storage) qualifier
        if (match(TokenType::Subroutine))
        {
            auto expr = std::make_unique<SubroutineQualifier>(previous());

            if (match(TokenType::LeftParen))
            {
                auto list = std::make_unique<ListExpr>("typeNames");
                expr->exprs.push_back(std::make_unique<Variable>(consume(TokenType::Identifier)));
                while (match(TokenType::Comma))
                    expr->exprs.push_back(std::make_unique<Variable>(consume(TokenType::Identifier)));

                consume(TokenType::RightParen);
                return expr;
            }

            return expr;
        }

        // Precision qualifier, interpolation, invariant, precise qualifiers
        if (match({ TokenType::LowP, TokenType::MediumP, TokenType::HighP,
            TokenType::Smooth, TokenType::Flat, TokenType::NoPerspective,
            TokenType::Invariant,
            TokenType::Precise }))
            return std::make_unique<TypeQualifier>(previous());

        // Otherwise, could be layout qualifier
        // or nullptr, if not
        return layoutQualifier();
    }

    std::vector<std::unique_ptr<TypeQualifier>> Parser::typeQualifierList()
    {
        std::vector<std::unique_ptr<TypeQualifier>> list;

        while (true)
        {
            if (auto expr = typeQualifier())
                list.push_back(std::move(expr));
            else
                break;
        }

        return list;
    }

    std::unique_ptr<TypeSpecifier> Parser::typeSpecifier()
    {
        auto expr = typeSpecifierNonarray();
        expr->arraySpecifiers = arraySpecifiers();
        return expr;
    }

    std::vector<std::unique_ptr<ArraySpecifier>> Parser::arraySpecifiers()
    {
        std::vector<std::unique_ptr<ArraySpecifier>> exprs;

        while (match(TokenType::LeftBracket))
        {
            if (match(TokenType::RightBracket))
            {
                exprs.emplace_back(std::make_unique<ArraySpecifier>());
            }
            else
            {
                exprs.emplace_back(std::make_unique<ArraySpecifier>(conditional()));
                consume(TokenType::RightBracket);
            }
        }

        return exprs;
    }

    std::unique_ptr<TypeSpecifier> Parser::typeSpecifierNonarray()
    {
        if (match({ TokenType::Void,
            TokenType::Float,
            TokenType::Double,
            TokenType::Int,
            TokenType::Uint,
            TokenType::Void,
            TokenType::Bool,
            TokenType::Mat2,
            TokenType::Mat3,
            TokenType::Mat4,
            TokenType::Dmat2,
            TokenType::Dmat3,
            TokenType::Dmat4,
            TokenType::Mat2x2,
            TokenType::Mat2x3,
            TokenType::Mat2x4,
            TokenType::Dmat2x2,
            TokenType::Dmat2x3,
            TokenType::Dmat2x4,
            TokenType::Mat3x2,
            TokenType::Mat3x3,
            TokenType::Mat3x4,
            TokenType::Dmat3x2,
            TokenType::Dmat3x3,
            TokenType::Dmat3x4,
            TokenType::Mat4x2,
            TokenType::Mat4x3,
            TokenType::Mat4x4,
            TokenType::Dmat4x2,
            TokenType::Dmat4x3,
            TokenType::Dmat4x4,
            TokenType::Vec2,
            TokenType::Vec3,
            TokenType::Vec4,
            TokenType::Ivec2,
            TokenType::Ivec3,
            TokenType::Ivec4,
            TokenType::Bvec2,
            TokenType::Bvec3,
            TokenType::Bvec4,
            TokenType::Uvec2,
            TokenType::Uvec3,
            TokenType::Uvec4,
            TokenType::Dvec2,
            TokenType::Dvec3,
            TokenType::Dvec4,
            TokenType::AtomicUint,

            TokenType::Sampler1D,
            TokenType::Sampler2D,
            TokenType::Sampler3D,
            TokenType::SamplerCube,
            TokenType::Sampler1DShadow,
            TokenType::Sampler2DShadow,
            TokenType::SamplerCubeShadow,
            TokenType::Sampler1DArray,
            TokenType::Sampler2DArray,
            TokenType::Sampler1DArrayShadow,
            TokenType::Sampler2DArrayShadow,
            TokenType::Sampler2DRect,
            TokenType::Sampler2DRectShadow,
            TokenType::SamplerBuffer,
            TokenType::Isampler1D,
            TokenType::Isampler2D,
            TokenType::Isampler3D,
            TokenType::IsamplerCube,
            TokenType::Isampler1DArray,
            TokenType::Isampler2DArray,
            TokenType::Isampler2DRect,
            TokenType::IsamplerBuffer,
            TokenType::Usampler1D,
            TokenType::Usampler2D,
            TokenType::Usampler3D,
            TokenType::UsamplerCube,
            TokenType::Usampler1DArray,
            TokenType::Usampler2DArray,
            TokenType::Usampler2DRect,
            TokenType::UsamplerBuffer,
            TokenType::Sampler2DMS,
            TokenType::Isampler2DMS,
            TokenType::Usampler2DMS,
            TokenType::Sampler2DMSArray,
            TokenType::Isampler2DMSArray,
            TokenType::Usampler2DMSArray,
            TokenType::SamplerCubeArray,
            TokenType::SamplerCubeArrayShadow,
            TokenType::IsamplerCubeArray,
            TokenType::UsamplerCubeArray,
            TokenType::Image1D,
            TokenType::Iimage1D,
            TokenType::Uimage1D,
            TokenType::Image2D,
            TokenType::Iimage2D,
            TokenType::Uimage2D,
            TokenType::Image3D,
            TokenType::Iimage3D,
            TokenType::Uimage3D,
            TokenType::Image2DRect,
            TokenType::Iimage2DRect,
            TokenType::Uimage2DRect,
            TokenType::ImageCube,
            TokenType::IimageCube,
            TokenType::UimageCube,
            TokenType::ImageBuffer,
            TokenType::IimageBuffer,
            TokenType::UimageBuffer,
            TokenType::Image1DArray,
            TokenType::Iimage1DArray,
            TokenType::Uimage1DArray,
            TokenType::Image2DArray,
            TokenType::Iimage2DArray,
            TokenType::Uimage2DArray,
            TokenType::ImageCubeArray,
            TokenType::IimageCubeArray,
            TokenType::UimageCubeArray,
            TokenType::Image2DMS,
            TokenType::Iimage2DMS,
            TokenType::Uimage2DMS,
            TokenType::Image2DMSArray,
            TokenType::Iimage2DMSArray,
            TokenType::Uimage2DMSArray }))
            return std::make_unique<TypeSpecifier>(previous());

        if (match(TokenType::Struct))
        {
            auto structSpec = std::make_unique<StructSpecifier>(previous());
            if (match(TokenType::Identifier))
                structSpec->structName = previous();
            consume(TokenType::LeftBrace);

            structSpec->fields.push_back(structFieldDeclaration());

            while (!match(TokenType::RightBrace))
                structSpec->fields.push_back(structFieldDeclaration());

            return structSpec;
        }
        else if (match(TokenType::Identifier))
            return std::make_unique<TypeSpecifier>(previous());

        throw std::runtime_error("Cannot parse type!");
    }

    std::unique_ptr<StructFieldDeclaration> Parser::structFieldDeclaration()
    {
        auto structField = std::make_unique<StructFieldDeclaration>();
        structField->fullType = fullySpecifiedType();
        structField->variable = std::make_unique<Variable>(consume(TokenType::Identifier));
        structField->variable->arraySpecifiers = arraySpecifiers();

        while (match(TokenType::Comma))
        {
            auto var = std::make_unique<Variable>(consume(TokenType::Identifier));
            var->arraySpecifiers = arraySpecifiers();

            structField->varList.push_back(std::move(var));
        }

        consume(TokenType::Semicolon);
        return structField;
    }

    std::unique_ptr<Expr> Parser::initializer()
    {
        if (match(TokenType::LeftBrace))
        {
            auto init = initializer();
            std::vector<ExprPtr> inits;
            while (match(TokenType::Comma))
                inits.push_back(initializer());

            match(TokenType::Comma);
            consume(TokenType::RightBrace);
            return init;
        }
        else
            return assignment();
    }

    std::unique_ptr<Statement> Parser::statement()
    {
        if (match(TokenType::LeftBrace))
        {
            auto block = std::make_unique<StatementBlock>("Block");
            while (!match(TokenType::RightBrace))
                block->statements.push_back(statement());
            return block;
        }
        else
            return simpleStatement();
    }

    std::unique_ptr<Statement> Parser::simpleStatement()
    {
        if (match(TokenType::If))
            return selectionStatement();
        else if (match(TokenType::Switch))
            return switchStatement();
        else if (match({ TokenType::Continue, TokenType::Break, TokenType::Discard, TokenType::Return }))
            return jumpStatement();

        if (auto d = declaration())
            return d;

        if (auto i = iterationStatement())
            return i;

        return expressionStatement();
    }

    std::unique_ptr<ExprStatement> Parser::expressionStatement()
    {
        if (match(TokenType::Semicolon))
            return std::make_unique<ExprStatement>(nullptr);

        auto s = std::make_unique<ExprStatement>(expression());
        consume(TokenType::Semicolon);
        return s;
    }

    std::unique_ptr<IfStatement> Parser::selectionStatement()
    {
        auto expr = std::make_unique<IfStatement>(previous());
        consume(TokenType::LeftParen);
        expr->condition = expression();
        consume(TokenType::RightParen);
        expr->thenBranch = statement();
        if (match(TokenType::Else))
            expr->elseBranch = statement();

        return expr;
    }

    std::unique_ptr<Expr> Parser::conditionExpr()
    {
        if (auto e = tryParse<Expr>(&Parser::expression))
            return e;
        else
        {
            auto type = fullySpecifiedType();
            consume(TokenType::Identifier);
            consume(TokenType::Equal);
            auto init = initializer();
            return init;
        }
    }

    std::unique_ptr<SwitchStatement> Parser::switchStatement()
    {
        auto stmt = std::make_unique<SwitchStatement>(previous());
        consume(TokenType::LeftParen);
        stmt->condition = expression();
        consume(TokenType::RightParen);
        consume(TokenType::LeftBrace);
        while (!match(TokenType::RightBrace))
            stmt->body.push_back(statement());

        return stmt;
    }

    std::unique_ptr<CaseLabel> Parser::caseLabel()
    {
        if (match(TokenType::Case))
        {
            auto lab = std::make_unique<CaseLabel>(previous());
            lab->condition = expression();
            consume(TokenType::FullColon);
            return lab;
        }

        auto lab = std::make_unique<CaseLabel>(consume(TokenType::Default));
        consume(TokenType::FullColon);
        return lab;
    }

    std::unique_ptr<LoopStatement> Parser::iterationStatement()
    {
        if (match(TokenType::While))
        {
            auto stmt = std::make_unique<LoopStatement>(previous());
            consume(TokenType::LeftParen);
            stmt->condition = conditionExpr();
            consume(TokenType::RightParen);
            stmt->body = statement();
            return stmt;
        }

        if (match(TokenType::Do))
        {
            auto stmt = std::make_unique<LoopStatement>(previous());
            stmt->body = statement();
            consume(TokenType::While);
            consume(TokenType::LeftParen);
            stmt->condition = expression();
            consume(TokenType::RightParen);
            consume(TokenType::Semicolon);
            return stmt;
        }

        if (match(TokenType::For))
        {
            auto stmt = std::make_unique<ForStatement>(previous());
            consume(TokenType::LeftParen);
            if (auto s = tryParse<Statement>(&Parser::expressionStatement))
                stmt->init = std::move(s);
            else
                stmt->init = declaration();

            if (!match(TokenType::Semicolon))
            {
                stmt->condition = conditionExpr();
                consume(TokenType::Semicolon);
            }

            if (!match(TokenType::RightParen))
            {
                stmt->increment = expression();
                consume(TokenType::RightParen);
            }

            stmt->body = statement();
            return stmt;
        }

        return nullptr;
    }

    std::unique_ptr<JumpStatement> Parser::jumpStatement()
    {
        auto stmt = std::make_unique<JumpStatement>(previous());
        if (previous().type != TokenType::Return)
        {
            consume(TokenType::Semicolon);
        }
        else if (!match(TokenType::Semicolon))
        {
            stmt->result = expression();
            consume(TokenType::Semicolon);
        }

        return stmt;
    }

    std::unique_ptr<Statement> Parser::externalDeclaration()
    {
        if (match(TokenType::Semicolon))
            return std::make_unique<ExprStatement>(nullptr);

        if (auto decl = tryParse<Statement>(&Parser::declaration))
            return decl;

        return functionDefinition();
    }

    std::unique_ptr<FunctionDefinition> Parser::functionDefinition()
    {
        auto func = std::make_unique<FunctionDefinition>();
        func->prototype = functionPrototype();

        consume(TokenType::LeftBrace);
        func->scope = std::make_unique<StatementBlock>("funcBody");
        while (!match(TokenType::RightBrace))
            func->scope->statements.push_back(statement());

        return func;
    }
}
