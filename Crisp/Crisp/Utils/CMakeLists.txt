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

add_cpp_interface_library(CrispProfiler
    "ChromeProfiler.hpp"
    "Profiler.hpp"
)
target_link_libraries(CrispProfiler
    INTERFACE Crisp::Format
)

add_cpp_interface_library(CrispGlmFormat
    "GlmFormat.hpp"
)
target_link_libraries(CrispGlmFormat
    INTERFACE Crisp::Math
    INTERFACE Crisp::Format
)

add_cpp_interface_library(CrispTimer
    "Timer.hpp"
)

add_cpp_test(CrispGlmFormatTest
    "Test/GlmStructuredBindingsTest.cpp"
)
target_link_libraries(CrispGlmFormatTest PRIVATE Crisp::GlmFormat)
