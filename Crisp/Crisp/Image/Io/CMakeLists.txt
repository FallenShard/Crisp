add_cpp_static_library(CrispImageIo
    "Utils.cpp"
    "Utils.hpp"
    "OpenEXRReader.cpp"
    "OpenEXRReader.hpp"
    "OpenEXRWriter.cpp"
    "OpenEXRWriter.hpp")
target_link_libraries(CrispImageIo
    PUBLIC Crisp::Result
    PUBLIC Crisp::Image
    PRIVATE Crisp::Logger
    PRIVATE Crisp::Format
    PRIVATE tinyexr
    PRIVATE stb)
