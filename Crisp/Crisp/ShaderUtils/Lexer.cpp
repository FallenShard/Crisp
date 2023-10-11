#include <Crisp/ShaderUtils/Lexer.hpp>

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/IO/FileUtils.hpp>

#include <Crisp/Core/Logger.hpp>

#include <charconv>

namespace {
robin_hood::unordered_flat_map<std::string, crisp::sl::TokenType> createKeywordMap() {
    using crisp::sl::TokenType;
    robin_hood::unordered_flat_map<std::string, crisp::sl::TokenType> map;

    // Execution Control
    map.emplace("break", TokenType::Break);
    map.emplace("continue", TokenType::Continue);
    map.emplace("do", TokenType::Do);
    map.emplace("while", TokenType::While);
    map.emplace("for", TokenType::For);
    map.emplace("switch", TokenType::Switch);
    map.emplace("case", TokenType::Case);
    map.emplace("default", TokenType::Default);
    map.emplace("if", TokenType::If);
    map.emplace("else", TokenType::Else);
    map.emplace("subroutine", TokenType::Subroutine);
    map.emplace("discard", TokenType::Discard);
    map.emplace("return", TokenType::Return);

    // Qualifiers
    map.emplace("const", TokenType::Const);
    map.emplace("in", TokenType::In);
    map.emplace("out", TokenType::Out);
    map.emplace("inout", TokenType::InOut);

    // Types
    map.emplace("float", TokenType::Float);
    map.emplace("double", TokenType::Double);
    map.emplace("int", TokenType::Int);
    map.emplace("uint", TokenType::Uint);
    map.emplace("void", TokenType::Void);
    map.emplace("bool", TokenType::Bool);
    map.emplace("struct", TokenType::Struct);
    map.emplace("mat2", TokenType::Mat2);
    map.emplace("mat3", TokenType::Mat3);
    map.emplace("mat4", TokenType::Mat4);
    map.emplace("dmat2", TokenType::Dmat2);
    map.emplace("dmat3", TokenType::Dmat3);
    map.emplace("dmat4", TokenType::Dmat4);
    map.emplace("mat2x2", TokenType::Mat2x2);
    map.emplace("mat2x3", TokenType::Mat2x3);
    map.emplace("mat2x4", TokenType::Mat2x4);
    map.emplace("dmat2x2", TokenType::Dmat2x2);
    map.emplace("dmat2x3", TokenType::Dmat2x3);
    map.emplace("dmat2x4", TokenType::Dmat2x4);
    map.emplace("mat3x2", TokenType::Mat3x2);
    map.emplace("mat3x3", TokenType::Mat3x3);
    map.emplace("mat3x4", TokenType::Mat3x4);
    map.emplace("dmat3x2", TokenType::Dmat3x2);
    map.emplace("dmat3x3", TokenType::Dmat3x3);
    map.emplace("dmat3x4", TokenType::Dmat3x4);
    map.emplace("mat4x2", TokenType::Dmat4x2);
    map.emplace("mat4x3", TokenType::Dmat4x3);
    map.emplace("mat4x4", TokenType::Dmat4x4);
    map.emplace("dmat4x2", TokenType::Dmat4x2);
    map.emplace("dmat4x3", TokenType::Dmat4x3);
    map.emplace("dmat4x4", TokenType::Dmat4x4);
    map.emplace("vec2", TokenType::Vec2);
    map.emplace("vec3", TokenType::Vec3);
    map.emplace("vec4", TokenType::Vec4);
    map.emplace("ivec2", TokenType::Ivec2);
    map.emplace("ivec3", TokenType::Ivec3);
    map.emplace("ivec4", TokenType::Ivec4);
    map.emplace("bvec2", TokenType::Bvec2);
    map.emplace("bvec3", TokenType::Bvec3);
    map.emplace("bvec4", TokenType::Bvec4);
    map.emplace("uvec2", TokenType::Uvec2);
    map.emplace("uvec3", TokenType::Uvec3);
    map.emplace("uvec4", TokenType::Uvec4);
    map.emplace("dvec2", TokenType::Dvec2);
    map.emplace("dvec3", TokenType::Dvec3);
    map.emplace("dvec4", TokenType::Dvec4);

    // Declarations
    map.emplace("uniform", TokenType::Uniform);
    map.emplace("buffer", TokenType::Buffer);
    map.emplace("shared", TokenType::Shared);
    map.emplace("coherent", TokenType::Coherent);
    map.emplace("volatile", TokenType::Volatile);
    map.emplace("restrict", TokenType::Restrict);
    map.emplace("readonly", TokenType::ReadOnly);
    map.emplace("writeonly", TokenType::WriteOnly);
    map.emplace("atomic_uint", TokenType::AtomicUint);
    map.emplace("layout", TokenType::Layout);
    map.emplace("centroid", TokenType::Centroid);
    map.emplace("flat", TokenType::Flat);
    map.emplace("smooth", TokenType::Smooth);
    map.emplace("noperspective", TokenType::NoPerspective);
    map.emplace("patch", TokenType::Patch);
    map.emplace("sample", TokenType::Sample);
    map.emplace("invariant", TokenType::Invariant);
    map.emplace("precise", TokenType::Precise);
    map.emplace("lowp", TokenType::LowP);
    map.emplace("mediump", TokenType::MediumP);
    map.emplace("highp", TokenType::HighP);
    map.emplace("precision", TokenType::Precision);

    // Sampler types
    map.emplace("sampler1D", TokenType::Sampler1D);
    map.emplace("sampler2D", TokenType::Sampler2D);
    map.emplace("sampler3D", TokenType::Sampler3D);
    map.emplace("samplerCube", TokenType::SamplerCube);
    map.emplace("sampler1DShadow", TokenType::Sampler1DShadow);
    map.emplace("sampler2DShadow", TokenType::Sampler2DShadow);
    map.emplace("SamplerCubeShadow", TokenType::SamplerCubeShadow);
    map.emplace("sampler1DArray", TokenType::Sampler1DArray);
    map.emplace("sampler2DArray", TokenType::Sampler2DArray);
    map.emplace("sampler1DArrayShadow", TokenType::Sampler1DArrayShadow);
    map.emplace("sampler2DArrayShadow", TokenType::Sampler2DArrayShadow);
    map.emplace("sampler2DRect", TokenType::Sampler2DRect);
    map.emplace("sampler2DRectShadow", TokenType::Sampler2DRectShadow);
    map.emplace("samplerBuffer", TokenType::SamplerBuffer);
    map.emplace("isampler1D", TokenType::Isampler1D);
    map.emplace("isampler2D", TokenType::Isampler2D);
    map.emplace("isampler3D", TokenType::Isampler3D);
    map.emplace("isamplerCube", TokenType::IsamplerCube);
    map.emplace("isampler1DArray", TokenType::Isampler1DArray);
    map.emplace("isampler2DArray", TokenType::Isampler2DArray);
    map.emplace("isampler2DRect", TokenType::Isampler2DRect);
    map.emplace("isamplerBuffer", TokenType::IsamplerBuffer);
    map.emplace("usampler1D", TokenType::Usampler1D);
    map.emplace("usampler2D", TokenType::Usampler2D);
    map.emplace("usampler3D", TokenType::Usampler3D);
    map.emplace("usamplerCube", TokenType::UsamplerCube);
    map.emplace("usampler1DArray", TokenType::Usampler1DArray);
    map.emplace("usampler2DArray", TokenType::Usampler2DArray);
    map.emplace("usampler2DRect", TokenType::Usampler2DRect);
    map.emplace("usamplerBuffer", TokenType::UsamplerBuffer);

    map.emplace("sampler2DMS", TokenType::Sampler2DMS);
    map.emplace("isampler2DMS", TokenType::Isampler2DMS);
    map.emplace("usampler2DMS", TokenType::Usampler2DMS);

    map.emplace("sampler2DMSArray", TokenType::Sampler2DMSArray);
    map.emplace("isampler2DMSArray", TokenType::Isampler2DMSArray);
    map.emplace("usampler2DMSArray", TokenType::Usampler2DMSArray);

    map.emplace("samplerCubeArray", TokenType::SamplerCubeArray);
    map.emplace("samplerCubeArrayShadow", TokenType::SamplerCubeArrayShadow);

    map.emplace("isamplerCubeArray", TokenType::IsamplerCubeArray);
    map.emplace("usamplerCubeArray", TokenType::UsamplerCubeArray);

    map.emplace("image1D", TokenType::Image1D);
    map.emplace("iimage1D", TokenType::Iimage1D);
    map.emplace("uimage1D", TokenType::Uimage1D);

    map.emplace("image2D", TokenType::Image2D);
    map.emplace("iimage2D", TokenType::Iimage2D);
    map.emplace("uimage2D", TokenType::Uimage2D);

    map.emplace("image3D", TokenType::Image3D);
    map.emplace("iimage3D", TokenType::Iimage3D);
    map.emplace("uimage3D", TokenType::Uimage3D);

    map.emplace("image2DRect", TokenType::Image2DRect);
    map.emplace("iimage2DRect", TokenType::Iimage2DRect);
    map.emplace("uimage2DRect", TokenType::Uimage2DRect);

    map.emplace("imageCube", TokenType::ImageCube);
    map.emplace("iimageCube", TokenType::IimageCube);
    map.emplace("uimageCube", TokenType::UimageCube);

    map.emplace("imageBuffer", TokenType::ImageBuffer);
    map.emplace("iimageBuffer", TokenType::IimageBuffer);
    map.emplace("uimageBuffer", TokenType::UimageBuffer);

    map.emplace("image1DArray", TokenType::Image1DArray);
    map.emplace("iimage1DArray", TokenType::Iimage1DArray);
    map.emplace("uimage1DArray", TokenType::Uimage1DArray);

    map.emplace("image2DArray", TokenType::Image2DArray);
    map.emplace("iimage2DArray", TokenType::Iimage2DArray);
    map.emplace("uimage2DArray", TokenType::Uimage2DArray);

    map.emplace("imageCubeArray", TokenType::ImageCubeArray);
    map.emplace("iimageCubeArray", TokenType::IimageCubeArray);
    map.emplace("uimageCubeArray", TokenType::UimageCubeArray);

    map.emplace("image2DMS", TokenType::Image2DMS);
    map.emplace("iimage2DMS", TokenType::Iimage2DMS);
    map.emplace("uimage2DMS", TokenType::Uimage2DMS);

    map.emplace("image2DMSArray", TokenType::Image2DMSArray);
    map.emplace("iimage2DMSArray", TokenType::Iimage2DMSArray);
    map.emplace("uimage2DMSArray", TokenType::Uimage2DMSArray);

    map.emplace("true", TokenType::BoolConstant);
    map.emplace("false", TokenType::BoolConstant);

    map.emplace("texture1D", TokenType::Texture1D);
    map.emplace("texture1DArray", TokenType::Texture1DArray);
    map.emplace("iTexture1D", TokenType::ITexture1D);
    map.emplace("iTexture1DArray", TokenType::ITexture1DArray);
    map.emplace("uTexture1D", TokenType::UTexture1D);
    map.emplace("uTexture1DArray", TokenType::UTexture1DArray);
    map.emplace("texture2D", TokenType::Texture2D);
    map.emplace("texture2DArray", TokenType::Texture2DArray);
    map.emplace("iTexture2D", TokenType::ITexture2D);
    map.emplace("iTexture2DArray", TokenType::ITexture2DArray);
    map.emplace("uTexture2D", TokenType::UTexture2D);
    map.emplace("uTexture2DArray", TokenType::UTexture2DArray);
    map.emplace("texture2DRect", TokenType::Texture2DRect);
    map.emplace("iTexture2DRect", TokenType::ITexture2DRect);
    map.emplace("uTexture2DRect", TokenType::UTexture2DRect);
    map.emplace("texture2DMS", TokenType::Texture2DMS);
    map.emplace("iTexture2DMS", TokenType::ITexture2DMS);
    map.emplace("uTexture2DMS", TokenType::UTexture2DMS);
    map.emplace("texture2DMSArray", TokenType::Texture2DMSArray);
    map.emplace("uTexture2DMSArray", TokenType::ITexture2DMSArray);
    map.emplace("iTexture2DMSArray", TokenType::UTexture2DMSArray);
    map.emplace("texture3D", TokenType::Texture3D);
    map.emplace("uTexture3D", TokenType::ITexture3D);
    map.emplace("iTexture3D", TokenType::UTexture3D);
    map.emplace("textureCube", TokenType::TextureCube);
    map.emplace("uTextureCube", TokenType::ITextureCube);
    map.emplace("iTextureCube", TokenType::UTextureCube);
    map.emplace("textureCubeArray", TokenType::TextureCubeArray);
    map.emplace("iTextureCubeArray", TokenType::ITextureCubeArray);
    map.emplace("uTextureCubeArray", TokenType::UTextureCubeArray);
    map.emplace("textureBuffer", TokenType::TextureBuffer);
    map.emplace("iTextureBuffer", TokenType::ITextureBuffer);
    map.emplace("uTextureBuffer", TokenType::UTextureBuffer);

    map.emplace("sampler", TokenType::Sampler);
    map.emplace("samplerShadow", TokenType::SamplerShadow);
    map.emplace("subpassInput", TokenType::SubpassInput);
    map.emplace("iSubpassInput", TokenType::ISubpassInput);
    map.emplace("uSubpassInput", TokenType::USubpassInput);
    map.emplace("subpassInputMS", TokenType::SubpassInputMS);
    map.emplace("iSubpassInputMS", TokenType::ISubpassInputMS);
    map.emplace("uSubpassInputMS", TokenType::USubpassInputMS);

    return map;
}

robin_hood::unordered_flat_map<std::string, crisp::sl::TokenType> Keywords = createKeywordMap();
} // namespace

namespace crisp::sl {
Lexer::Lexer(const std::string& source)
    : m_source(source)
    , m_start(0)
    , m_current(0)
    , m_line(1) {}

Lexer::Lexer(std::string&& source)
    : m_source(std::move(source))
    , m_start(0)
    , m_current(0)
    , m_line(1) {}

std::vector<Token> Lexer::scanTokens() {
    while (!isAtEnd()) {
        m_start = m_current;
        scanToken();
    }

    m_tokens.emplace_back(TokenType::EndOfFile, "", nullptr, m_line);

    return std::move(m_tokens);
}

bool Lexer::isAtEnd() const {
    return m_current >= static_cast<int>(m_source.size());
}

void Lexer::scanToken() {
    char c = advance();
    switch (c) {
    case '(':
        addToken(TokenType::LeftParen);
        break;
    case ')':
        addToken(TokenType::RightParen);
        break;
    case '[':
        addToken(TokenType::LeftBracket);
        break;
    case ']':
        addToken(TokenType::RightBracket);
        break;
    case '{':
        addToken(TokenType::LeftBrace);
        break;
    case '}':
        addToken(TokenType::RightBrace);
        break;
    case '.':
        addToken(TokenType::Dot);
        break;
    case ',':
        addToken(TokenType::Comma);
        break;
    case '-':
        if (match('-')) {
            addToken(TokenType::MinusMinus);
        } else if (match('=')) {
            addToken(TokenType::MinusEqual);
        } else {
            addToken(TokenType::Minus);
        }
        break;

    case '+':
        if (match('+')) {
            addToken(TokenType::PlusPlus);
        } else if (match('=')) {
            addToken(TokenType::PlusEqual);
        } else {
            addToken(TokenType::Plus);
        }
        break;

    case '*':
        addToken(match('=') ? TokenType::StarEqual : TokenType::Star);
        break;
    case '%':
        addToken(TokenType::Percent);
        break;
    case ':':
        addToken(TokenType::FullColon);
        break;
    case ';':
        addToken(TokenType::Semicolon);
        break;
    case '?':
        addToken(TokenType::QuestionMark);
        break;
    case '~':
        addToken(TokenType::Tilde);
        break;
    case '#':
        while (peek() != '\n' && !isAtEnd()) {
            advance();
        }
        break;
    case '&':
        addToken(match('&') ? TokenType::LogicalAnd : TokenType::BitwiseAnd);
        break;
    case '|':
        addToken(match('|') ? TokenType::LogicalOr : TokenType::BitwiseOr);
        break;
    case '^':
        addToken(match('^') ? TokenType::LogicalXor : TokenType::BitwiseXor);
        break;
    case '!':
        addToken(match('=') ? TokenType::NotEqual : TokenType::ExclamationMark);
        break;
    case '=':
        addToken(match('=') ? TokenType::EqualEqual : TokenType::Equal);
        break;
    case '<':
        if (match('=')) {
            addToken(TokenType::LessThanEqual);
        } else if (match('<')) {
            addToken(TokenType::BitShiftLeft);
        } else {
            addToken(TokenType::LessThan);
        }
        break;

    case '>':
        if (match('=')) {
            addToken(TokenType::GreaterThanEqual);
        } else if (match('>')) {
            addToken(TokenType::BitShiftRight);
        } else {
            addToken(TokenType::GreaterThan);
        }
        break;

    case '/':
        if (match('/')) {
            while (peek() != '\n' && !isAtEnd()) {
                advance();
            }
        } else if (match('*')) {
            while (peek() != '*' || peekNext() != '/') {
                advance();
            }
            while (peek() != '\n') {
                advance();
            }
        } else if (match('=')) {
            addToken(TokenType::SlashEqual);
        } else {
            addToken(TokenType::Slash);
        }
        break;

    case '"':
        addString();
        break;
    case ' ':
    case '\r':
    case '\t':
        break;

    case '\n':
        m_line++;
        break;

    default:
        if (isDigit(c)) {
            addNumber();
        } else if (isAlpha(c)) {
            addIdentifier();
        } else {
            spdlog::error("{} Unexpected character: {}", m_line, c);
        }
    }
}

char Lexer::peek() const {
    if (isAtEnd()) {
        return '\0';
    }
    return m_source[m_current];
}

char Lexer::peekNext() const {
    if (m_current + 1 >= static_cast<int32_t>(m_source.size())) {
        return '\0';
    }
    return m_source[m_current + 1];
}

char Lexer::advance() {
    return m_source[m_current++];
}

bool Lexer::match(char expected) {
    if (isAtEnd()) {
        return false;
    }

    if (m_source[m_current] != expected) {
        return false;
    }

    m_current++;
    return true;
}

bool Lexer::isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
}

bool Lexer::isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::isHexadecimalDigit(char c) {
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

void Lexer::addToken(TokenType type) {
    addToken(type, {});
}

void Lexer::addToken(TokenType type, std::any literal) {
    std::string lexeme = m_source.substr(m_start, m_current - m_start);
    m_tokens.emplace_back(type, lexeme, std::move(literal), m_line);
}

void Lexer::addString() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') {
            m_line++;
        }

        advance();
    }

    if (!isAtEnd()) {
        spdlog::error("Unexpected string at line: {}", m_line);
        return;
    }

    advance();

    std::string value = m_source.substr(m_start + 1, m_current - m_start - 2);
    addToken(TokenType::String, value);
}

void Lexer::addNumber() {
    int startOffset = 0;
    int base = 10;
    if (m_source[m_start] == '0') {
        startOffset = 1;
        base = 8;
    }

    if (match('x')) {
        startOffset = 2;
        base = 16;

        while (isHexadecimalDigit(peek())) {
            advance();
        }
    } else {
        while (isDigit(peek())) {
            advance();
        }
    }

    if (peek() == '.' && isDigit(peekNext())) {
        advance();

        while (isDigit(peek())) {
            advance();
        }

        if (peek() == 'e' || peek() == 'E') {
            advance();

            if (peek() == '+' || peek() == '-') {
                advance();
            }

            while (isDigit(peek())) {
                advance();
            }
        }

        if (peek() == 'f' || peek() == 'F') {
            float value = 0;
            std::from_chars(&m_source[m_start], &m_source[m_current], value);
            addToken(TokenType::FloatConstant, value);

            advance();
        } else {
            double value = 0;
            std::from_chars(&m_source[m_start], &m_source[m_current], value);
            addToken(TokenType::DoubleConstant, value);
        }
    } else {
        if (peek() == 'u') {
            unsigned int value = 0;
            std::from_chars(&m_source[m_start + startOffset], &m_source[m_current], value, base);
            addToken(TokenType::UintConstant, value);
            advance();
        } else {
            int value = 0;
            std::from_chars(&m_source[m_start + startOffset], &m_source[m_current], value, base);
            addToken(TokenType::IntConstant, value);
        }
    }
}

void Lexer::addIdentifier() {
    while (isAlphaNumeric(peek())) {
        advance();
    }

    std::string text = m_source.substr(m_start, m_current - m_start);
    TokenType type = TokenType::Identifier;
    auto iter = Keywords.find(text);
    if (iter != Keywords.end()) {
        type = iter->second;
    }

    std::any literal;
    if (text == "true") {
        literal = true;
    } else if (text == "false") {
        literal = false;
    }

    addToken(type, literal);
}
} // namespace crisp::sl
