add_cpp_interface_library(CrispFormat
    "Format.hpp"
)
target_link_libraries(CrispFormat
    INTERFACE fmt::fmt-header-only
)

add_cpp_interface_library(CrispHashMap
    "HashMap.hpp"
)
target_link_libraries(CrispHashMap
    INTERFACE robin_hood
)

add_cpp_interface_library(CrispLogger
    "Logger.hpp"
)
target_link_libraries(CrispLogger
    INTERFACE spdlog::spdlog_header_only
    INTERFACE CrispFormat
)

add_cpp_interface_library(CrispResult
    "Result.hpp"
)
target_link_libraries(CrispResult
    INTERFACE CrispLogger
)

add_cpp_interface_library(CrispResultTestUtils
    "Test/ResultTest.cpp")
target_link_libraries(CrispResultTestUtils
    INTERFACE CrispResult GTest::gmock
)

add_cpp_test(CrispResultTest
    "Test/ResultTest.cpp"
)
target_link_libraries(CrispResultTest
    PRIVATE Crisp::Result
)

add_cpp_interface_library(CrispChecks
    "Checks.hpp"
)
target_link_libraries(CrispChecks
    INTERFACE CrispResult
)

add_cpp_static_library(CrispWindow
    "Window.cpp"
    "Window.hpp"
    "Mouse.hpp"
    "Keyboard.hpp"
    "InputTranslator.hpp"
)
target_link_libraries(CrispWindow
    PUBLIC CrispBitFlags
    PUBLIC CrispMath
    PUBLIC CrispEvent
    PUBLIC CrispInput
    PUBLIC glfw
    PUBLIC Vulkan::Headers
)

add_cpp_interface_library(CrispInput
    "Mouse.hpp"
    "Keyboard.hpp"
    "InputTranslator.hpp"
)
target_link_libraries(CrispInput
    INTERFACE CrispBitFlags
    INTERFACE glfw
)

add_cpp_static_library(CrispApplicationEnvironment
    "ApplicationEnvironment.cpp"
    "ApplicationEnvironment.hpp"
)
target_link_libraries(CrispApplicationEnvironment
    PUBLIC CrispUtils
    PRIVATE CrispLogger
    PRIVATE CrispFormat
    PRIVATE CrispCommandLineParser
    PUBLIC CrispJsonUtils
    PUBLIC CrispHashMap
    PUBLIC CrispStringUtils
    PUBLIC glfw
    PUBLIC Vulkan::Headers
)

add_cpp_static_library(CrispCommandLineParser
    "CommandLineParser.cpp"
    "CommandLineParser.hpp"
)
target_link_libraries(CrispCommandLineParser
    PUBLIC CrispFormat
    PUBLIC CrispHashMap
    PUBLIC CrispStringUtils
    PUBLIC CrispLogger
)

add_cpp_test(CrispCommandLineParserTest
    "Test/CommandLineParserTest.cpp"
)
target_link_libraries(CrispCommandLineParserTest
    PRIVATE CrispCommandLineParser
)

add_cpp_static_library(CrispApplication
    "Application.cpp"
    "Application.hpp"
)
target_link_libraries(CrispApplication
    PUBLIC CrispBitFlags
    PUBLIC CrispMath
    PUBLIC CrispGui
    PUBLIC CrispWindow
    PUBLIC CrispApplicationEnvironment
    PUBLIC CrispRenderer
    PUBLIC CrispScenes
    PUBLIC CrispEvent
    PUBLIC CrispUtils
    PUBLIC glfw
    PUBLIC Vulkan::Headers
    PUBLIC CrispImGui
)

add_cpp_interface_library(CrispCoreUtils
    "ThreadPool.hpp"
)
target_link_libraries(CrispCoreUtils
    INTERFACE CrispUtils
    INTERFACE MPMCQueue
)

add_cpp_interface_library(CrispEvent
    "Event.hpp"
    "Delegate.hpp"
    "ConnectionHandler.hpp"
)
target_link_libraries(CrispEvent
    INTERFACE CrispHashMap
)

add_cpp_test(CrispEventTest
    "Test/DelegateTest.cpp"
    "Test/EventTest.cpp"
)
target_link_libraries(CrispEventTest
    PRIVATE CrispEvent
)

add_cpp_interface_library(CrispNotNull
    "NotNull.hpp"
)
target_link_libraries(CrispNotNull
    INTERFACE CrispChecks
)

add_cpp_interface_library(CrispSlotMap
    "SlotMap.hpp"
)
target_link_libraries(CrispSlotMap
    INTERFACE CrispHashMap
)

add_cpp_test(CrispSlotMapTest
    "Test/SlotMapTest.cpp"
)
target_link_libraries(CrispSlotMapTest
    PRIVATE CrispSlotMap
)