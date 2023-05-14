add_cpp_interface_library(CrispFormat
    "Format.hpp")
target_link_libraries(CrispFormat
    INTERFACE fmt::fmt-header-only)


add_cpp_interface_library(CrispHashMap
    "HashMap.hpp")
target_link_libraries(CrispHashMap
    INTERFACE robin_hood)


add_cpp_interface_library(CrispLogger
    "Logger.hpp")
target_link_libraries(CrispLogger
    INTERFACE spdlog::spdlog_header_only
    INTERFACE CrispFormat)


add_cpp_interface_library(CrispResult
    "Result.hpp")
target_link_libraries(CrispResult
    INTERFACE CrispLogger)

add_cpp_test(CrispResultTest
    "Test/ResultTest.cpp")
target_link_libraries(CrispResultTest PRIVATE CrispResult)

add_cpp_interface_library(CrispChecks
    "Checks.hpp")
target_link_libraries(CrispChecks
    INTERFACE CrispResult)
