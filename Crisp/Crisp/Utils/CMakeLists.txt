add_cpp_static_library(CrispStringUtils
    "StringUtils.cpp"
    "StringUtils.hpp"
)

add_cpp_test(CrispStringUtilsTest
    "Test/StringUtilsTest.cpp")
target_link_libraries(CrispStringUtilsTest PRIVATE Crisp::StringUtils)

add_cpp_interface_library(CrispBitFlags
    "BitFlags.hpp"
)
add_cpp_test(CrispBitFlagsTest
    "Test/BitFlagsTest.cpp")
target_link_libraries(CrispBitFlagsTest PRIVATE Crisp::BitFlags)
