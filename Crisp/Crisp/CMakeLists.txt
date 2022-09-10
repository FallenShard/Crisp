add_cpp_interface_library(CrispEvent
    "Event.hpp"
    "Delegate.hpp"
    "ConnectionHandler.hpp"
)
target_link_libraries(CrispEvent
    INTERFACE CrispHashMap)


add_cpp_interface_library(CrispBitFlags
    "BitFlags.hpp"
)


add_cpp_static_library(CrispStringUtils
    "StringUtils.cpp"
    "StringUtils.hpp"
)


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
target_link_libraries(CrispUtils
    INTERFACE CrispMath
    INTERFACE CrispFormat
    INTERFACE CrispLogger
    INTERFACE lua)