add_cpp_interface_library(CrispEvent
    "Event.hpp"
    "Delegate.hpp"
    "ConnectionHandler.hpp")
set_target_properties(CrispEvent PROPERTIES CXX_STANDARD 20 LINKER_LANGUAGE CXX)
target_include_directories(CrispEvent INTERFACE "../")
target_link_libraries(CrispEvent
    INTERFACE CrispRobinHood)


add_cpp_interface_library(CrispBitFlags
    "BitFlags.hpp")
set_target_properties(CrispBitFlags PROPERTIES CXX_STANDARD 20 LINKER_LANGUAGE CXX)
target_include_directories(CrispBitFlags INTERFACE "../")


add_cpp_static_library(CrispStringUtils
    "StringUtils.cpp"
    "StringUtils.hpp")
target_include_directories(CrispStringUtils PUBLIC "../")


add_cpp_interface_library(CrispUtils
    "ChromeProfiler.hpp"
    "Enumerate.hpp"
    "GlmFormatters.hpp"
    "LuaConfig.hpp"
    "MemoryRegion.hpp"
    "Profiler.hpp"
    "Timer.hpp"
    "ZipIterator.hpp"
    "Coroutines/Task.hpp"
    )
target_include_directories(CrispUtils INTERFACE ".")
target_link_libraries(CrispUtils
    INTERFACE CrispMath
    INTERFACE CrispFormat
    INTERFACE CrispLogger
    INTERFACE lua)