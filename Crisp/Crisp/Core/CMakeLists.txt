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
    INTERFACE unordered_dense
)

add_cpp_interface_library(CrispLogger
    "Logger.hpp"
)
target_link_libraries(CrispLogger
    INTERFACE spdlog::spdlog_header_only
    INTERFACE Crisp::Format
)

add_cpp_interface_library(CrispResult
    "Result.hpp"
)
target_link_libraries(CrispResult
    INTERFACE Crisp::Logger
)

add_cpp_interface_library(CrispResultTestUtils
    "Test/ResultTestUtils.hpp")
target_link_libraries(CrispResultTestUtils
    INTERFACE Crisp::Result GTest::gmock
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
    INTERFACE Crisp::Result
)

add_cpp_static_library(CrispWindow
    "Window.cpp"
    "Window.hpp"
)
target_link_libraries(CrispWindow
    PUBLIC Crisp::BitFlags
    PUBLIC Crisp::Math
    PUBLIC Crisp::Event
    PUBLIC Crisp::Input
    PUBLIC glfw
    PUBLIC Crisp::VulkanHeader
    PRIVATE Crisp::Logger
)

add_cpp_test(CrispWindowTest
    "Test/WindowTest.cpp"
)
target_link_libraries(CrispWindowTest
    PRIVATE Crisp::Window
)

add_cpp_interface_library(CrispInput
    "Mouse.hpp"
    "Keyboard.hpp"
    "InputTranslator.hpp"
)
target_link_libraries(CrispInput
    INTERFACE Crisp::BitFlags
    INTERFACE glfw
)

add_cpp_static_library(CrispApplicationEnvironment
    "ApplicationEnvironment.cpp"
    "ApplicationEnvironment.hpp"
)
target_link_libraries(CrispApplicationEnvironment
    PUBLIC Crisp::JsonUtils
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::StringUtils
    PUBLIC glfw
    PRIVATE Crisp::ChromeEventTracer
    PRIVATE Crisp::Logger
    PRIVATE Crisp::Format
    PRIVATE Crisp::CommandLineParser
)

add_cpp_static_library(CrispCommandLineParser
    "CommandLineParser.cpp"
    "CommandLineParser.hpp"
)
target_link_libraries(CrispCommandLineParser
    PUBLIC Crisp::Format
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::StringUtils
    PUBLIC Crisp::Logger
)

add_cpp_test(CrispCommandLineParserTest
    "Test/CommandLineParserTest.cpp"
)
target_link_libraries(CrispCommandLineParserTest
    PRIVATE Crisp::CommandLineParser
)

add_cpp_static_library(CrispApplication
    "Application.cpp"
    "Application.hpp"
)
target_link_libraries(CrispApplication
    PUBLIC Crisp::Window
    PUBLIC Crisp::ApplicationEnvironment
    PUBLIC Crisp::Event
    PUBLIC Crisp::Gui
    PUBLIC Crisp::Renderer
    PUBLIC Crisp::Scenes
    PUBLIC Crisp::ImGui
    PRIVATE Crisp::Timer
)

add_cpp_interface_library(CrispThreadPool
    "ThreadPool.hpp"
)
target_link_libraries(CrispThreadPool
    INTERFACE MPMCQueue
    INTERFACE Crisp::ScopeProfiler
)

add_cpp_test(CrispThreadPoolTest
    "Test/ThreadPoolTest.cpp"
)
target_link_libraries(CrispThreadPoolTest
    PRIVATE Crisp::ThreadPool
)

add_cpp_benchmark(CrispThreadPoolBenchmark
    "Benchmark/ThreadPoolBenchmark.cpp"
)
target_link_libraries(CrispThreadPoolBenchmark
    PRIVATE Crisp::ThreadPool
)

add_cpp_interface_library(CrispEvent
    "Event.hpp"
    "Delegate.hpp"
    "ConnectionHandler.hpp"
)
target_link_libraries(CrispEvent
    INTERFACE Crisp::HashMap
)

add_cpp_test(CrispEventTest
    "Test/DelegateTest.cpp"
    "Test/EventTest.cpp"
)
target_link_libraries(CrispEventTest
    PRIVATE Crisp::Event
)

add_cpp_interface_library(CrispSlotMap
    "SlotMap.hpp"
)

add_cpp_test(CrispSlotMapTest
    "Test/SlotMapTest.cpp"
)
target_link_libraries(CrispSlotMapTest
    PRIVATE Crisp::SlotMap
)

add_cpp_static_library(CrispChromeEventTracer
    "ChromeEventTracer.cpp"
    "ChromeEventTracer.hpp"
)
target_link_libraries(CrispChromeEventTracer
    PUBLIC Crisp::Format
    PUBLIC Crisp::ScopeProfiler
)
add_cpp_test(CrispChromeEventTracerTest
    "Test/ChromeEventTracerTest.cpp"
)
target_link_libraries(CrispChromeEventTracerTest
    PRIVATE Crisp::ChromeEventTracer
)

add_cpp_interface_library(CrispScopeProfiler
    "Profiler.hpp"
)
target_link_libraries(CrispScopeProfiler
    INTERFACE Crisp::Logger
)

add_cpp_interface_library(CrispTimer
    "Timer.hpp"
)