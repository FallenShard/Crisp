add_cpp_static_library(
    CrispFileUtils
    "FileUtils.cpp"
    "FileUtils.hpp"
)
target_link_libraries(
    CrispFileUtils
    PUBLIC Crisp::Result
    PUBLIC Crisp::Format
)

add_cpp_header_library(
    CrispJsonUtils
    "JsonUtils.hpp"
)
target_link_libraries(
    CrispJsonUtils
    INTERFACE Crisp::FileUtils
    INTERFACE nlohmann_json::nlohmann_json
)

add_cpp_test(
    CrispJsonUtilsTest
    "Test/JsonUtilsTest.cpp"
)
target_link_libraries(
    CrispJsonUtilsTest
    PRIVATE Crisp::JsonUtils
    PRIVATE Crisp::UniqueTemporaryFile
)
