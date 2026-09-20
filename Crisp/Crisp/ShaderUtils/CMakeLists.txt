add_cpp_static_library(CrispGlslUtils
    "Expressions.hpp"
    "Lexer.cpp"
    "Lexer.hpp"
    "Parser.cpp"
    "Parser.hpp"
    "PrintVisitor.hpp"

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
    PUBLIC Crisp::VulkanHeader
)

add_cpp_test(CrispGlslUtilsTest
    "Test/LexerTest.cpp")
target_link_libraries(CrispGlslUtilsTest PRIVATE Crisp::GlslUtils)

add_cpp_static_library(CrispShaderType
    "ShaderType.cpp"
    "ShaderType.hpp"
)
target_link_libraries(CrispShaderType
    PUBLIC Crisp::Result
    PRIVATE Crisp::HashMap
    PUBLIC Crisp::VulkanHeader
)

add_cpp_static_library(CrispShaderCompiler
    "ShaderCompiler.cpp"
    "ShaderCompiler.hpp"
)
target_link_libraries(CrispShaderCompiler
    PUBLIC Crisp::Result
    PRIVATE Crisp::FileUtils
    PRIVATE Crisp::HashMap
    PRIVATE Crisp::ShaderType
    PRIVATE Crisp::UniqueTemporaryFile
    PUBLIC Crisp::VulkanHeader
    PRIVATE glslang::glslang
    PRIVATE glslang::glslang-default-resource-limits
)

add_cpp_test(
    CrispShaderCompilerTest
    "Test/ShaderCompilerTest.cpp"
    DEPS
    Crisp::ShaderCompiler
    Crisp::ShaderType
    Crisp::UniqueTemporaryFile
    ASSET_DIR
    "Test/Data"
)

add_cpp_static_library(CrispSpvReflection
    "Reflection.cpp"
    "Reflection.hpp")
target_link_libraries(CrispSpvReflection
    PUBLIC Crisp::Result
    PUBLIC Crisp::VulkanHeader
    PRIVATE Crisp::Checks
    PUBLIC spirv-reflect-static
)

add_cpp_test(
    CrispSpvReflectionTest
    "Test/ReflectionTest.cpp"
    DEPS
    Crisp::SpvReflection
    Crisp::ShaderCompiler
    Crisp::UniqueTemporaryFile
    ASSET_DIR
    "Test/Data"
)
