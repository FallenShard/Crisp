add_cpp_static_library(CrispWindow
    "Window.cpp"
    "Window.hpp"
    "Mouse.hpp"
    "Keyboard.hpp"
    "InputTranslator.hpp")
target_include_directories(CrispWindow PUBLIC "../../")
target_link_libraries(CrispWindow
    PUBLIC CrispBitFlags
    PUBLIC CrispMath
    PUBLIC CrispEvent
    PUBLIC CrispInput
    PUBLIC glfw
    PUBLIC vulkan)


add_cpp_interface_library(CrispInput
    "Mouse.hpp"
    "Keyboard.hpp"
    "InputTranslator.hpp")
target_include_directories(CrispInput INTERFACE "../../")
target_link_libraries(CrispInput
    INTERFACE CrispBitFlags
    INTERFACE glfw)


add_cpp_static_library(CrispApplicationEnvironment
    "ApplicationEnvironment.cpp"
    "ApplicationEnvironment.hpp"
    "CommandLineParser.cpp"
    "CommandLineParser.hpp")
target_include_directories(CrispApplicationEnvironment PUBLIC "../../")
target_link_libraries(CrispApplicationEnvironment
    PUBLIC CrispUtils
    PUBLIC CrispLogger
    PUBLIC CrispRobinHood
    PUBLIC CrispStringUtils
    PUBLIC glfw
    PUBLIC vulkan)


add_cpp_static_library(CrispApplication
    "Application.cpp"
    "Application.hpp")
target_include_directories(CrispApplication PUBLIC "../../")
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
    PUBLIC vulkan)

add_cpp_interface_library(CrispCoreUtils
    "ThreadPool.hpp")
target_include_directories(CrispCoreUtils INTERFACE "../../")
target_link_libraries(CrispCoreUtils
    INTERFACE CrispUtils
    INTERFACE MPMCQueue)