add_cpp_static_library(CrispImageIo
    "Common.hpp"
    "Utils.cpp"
    "Utils.hpp"
    "Exr.cpp"
    "Exr.hpp")
target_link_libraries(CrispImageIo
    PUBLIC Crisp::Result
    PUBLIC Crisp::Image
    PRIVATE Crisp::Logger
    PRIVATE Crisp::Format
    PRIVATE tinyexr
    PRIVATE stb)
