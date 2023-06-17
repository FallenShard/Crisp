add_cpp_static_library(CrispGlslUtils
    "Expressions.hpp"
    "Lexer.cpp"
    "Lexer.hpp"
    "Parser.cpp"
    "Parser.hpp"
    "PrintVisitor.hpp"
    "Reflection.cpp"
    "Reflection.hpp"
    "ShaderCompiler.cpp"
    "ShaderCompiler.hpp"
    "ShaderType.cpp"
    "ShaderType.hpp"
    "Statements.hpp"
    "Token.hpp"
    "Visitor.hpp")
target_link_libraries(CrispGlslUtils
    PUBLIC Crisp::Math
    PUBLIC Crisp::Result
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::FileUtils
    PUBLIC Crisp::Utils
    PUBLIC Vulkan::Headers
    PUBLIC spirv-reflect-static
)

add_cpp_test(CrispGlslUtilsTest
    "Test/LexerTest.cpp")
target_link_libraries(CrispGlslUtilsTest PRIVATE Crisp::GlslUtils)

add_cpp_test(CrispGlslReflectionTest
    "Test/ReflectionTest.cpp")
target_link_libraries(CrispGlslReflectionTest PRIVATE Crisp::GlslUtils)

add_cpp_test(CrispShaderCompilerTest
    "Test/ShaderCompilerTest.cpp")
target_link_libraries(CrispShaderCompilerTest PRIVATE Crisp::GlslUtils)
