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
    PRIVATE glfw
    PUBLIC Vulkan::Headers
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
    PUBLIC Crisp::Utils
    PRIVATE Crisp::Logger
    PRIVATE Crisp::Format
    PRIVATE Crisp::CommandLineParser
    PUBLIC Crisp::JsonUtils
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::StringUtils
    PUBLIC glfw
    PUBLIC Vulkan::Headers
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
)

add_cpp_interface_library(CrispCoreUtils
    "ThreadPool.hpp"
)
target_link_libraries(CrispCoreUtils
    INTERFACE Crisp::Utils
    INTERFACE MPMCQueue
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

add_cpp_interface_library(CrispNotNull
    "NotNull.hpp"
)
target_link_libraries(CrispNotNull
    INTERFACE Crisp::Checks
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