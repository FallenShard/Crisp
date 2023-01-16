add_cpp_static_library(CrispImageIo
    "Utils.cpp"
    "Utils.hpp"
    "OpenEXRReader.cpp"
    "OpenEXRReader.hpp"
    "OpenEXRWriter.cpp"
    "OpenEXRWriter.hpp")
target_link_libraries(CrispImageIo
    PUBLIC CrispResult
    PUBLIC CrispImage
    PRIVATE CrispLogger
    PRIVATE CrispFormat
    PRIVATE tinyexr
    PRIVATE stb)
