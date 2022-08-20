add_cpp_interface_library(CrispFormat
    "Format.hpp")
target_include_directories(CrispFormat INTERFACE "../../")
target_link_libraries(CrispFormat
    INTERFACE fmt)


add_cpp_interface_library(CrispRobinHood
    "RobinHood.hpp")
target_include_directories(CrispRobinHood INTERFACE "../../")
target_link_libraries(CrispRobinHood
    INTERFACE robin_hood)


add_cpp_interface_library(CrispLogger
    "Logger.hpp")
target_include_directories(CrispLogger INTERFACE "../../")
target_link_libraries(CrispLogger
    INTERFACE spdlog
    INTERFACE CrispFormat)


add_cpp_interface_library(CrispResult
    "Result.hpp")
target_include_directories(CrispResult INTERFACE "../../")
target_link_libraries(CrispResult
    INTERFACE CrispLogger)