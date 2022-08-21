add_cpp_interface_library(CrispFormat
    "Format.hpp")
target_link_libraries(CrispFormat
    INTERFACE fmt)


add_cpp_interface_library(CrispRobinHood
    "RobinHood.hpp")
target_link_libraries(CrispRobinHood
    INTERFACE robin_hood)


add_cpp_interface_library(CrispLogger
    "Logger.hpp")
target_link_libraries(CrispLogger
    INTERFACE spdlog
    INTERFACE CrispFormat)


add_cpp_interface_library(CrispResult
    "Result.hpp")
target_link_libraries(CrispResult
    INTERFACE CrispLogger)