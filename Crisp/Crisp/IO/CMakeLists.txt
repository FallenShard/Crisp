add_cpp_static_library(CrispFileUtils
    "FileUtils.cpp"
    "FileUtils.hpp")
target_link_libraries(CrispFileUtils
    PUBLIC CrispResult
    PUBLIC CrispFormat)


add_cpp_static_library(CrispFontIo
    "FontLoader.cpp"
    "FontLoader.hpp")
target_link_libraries(CrispFontIo
    PUBLIC freetype
    PRIVATE CrispLogger)


add_cpp_interface_library(CrispJsonUtils
    "JsonUtils.hpp")
target_link_libraries(CrispJsonUtils
    INTERFACE CrispFileUtils
    INTERFACE nlohmann_json::nlohmann_json)