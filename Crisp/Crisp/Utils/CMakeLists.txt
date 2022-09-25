add_cpp_static_library(CrispStringUtils
    "StringUtils.cpp"
    "StringUtils.hpp"
)

add_cpp_test(CrispStringUtilsTest
    "Test/StringUtilsTest.cpp")
target_link_libraries(CrispStringUtilsTest PRIVATE CrispStringUtils)


add_cpp_interface_library(CrispBitFlags
    "BitFlags.hpp"
)
add_cpp_test(CrispBitFlagsTest
    "Test/BitFlagsTest.cpp")
target_link_libraries(CrispBitFlagsTest PRIVATE CrispBitFlags)

add_cpp_interface_library(CrispUtils
    "ChromeProfiler.hpp"
    "Enumerate.hpp"
    "GlmFormatters.hpp"
    "LuaConfig.hpp"
    "Profiler.hpp"
    "Timer.hpp"
    "ZipIterator.hpp"
    "Task.hpp"
)
target_link_libraries(CrispUtils
    INTERFACE CrispMath
    INTERFACE CrispFormat
    INTERFACE CrispLogger
    INTERFACE lua)

add_cpp_test(CrispUtilsTest
    "Test/GlmStructuredBindingsTest.cpp"
    "Test/LuaConfigTest.cpp")
target_link_libraries(CrispUtilsTest PRIVATE CrispUtils)
copy_shared_libs(CrispUtilsTest lua)

