cmake_minimum_required(VERSION 3.28...3.30)
project(Crisp VERSION 0.1.0 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(CRISP_BUILD_TESTS      "Build Crisp unit tests"  ON)
option(CRISP_BUILD_BENCHMARKS "Build Crisp benchmarks"  ON)

if(CRISP_BUILD_TESTS)
    include(CTest)
    include(GoogleTest)
endif()

include(cmake/TargetHelpers.cmake)
include(cmake/Dependencies.cmake)

add_subdirectory(Crisp)

# clangd auto-discovers compile_commands.json in an ancestor directory or in that ancestor's
# build/ subdirectory, but the presets nest the build tree one level deeper in
# build/<presetName>. Link the database up into build/ so clangd follows whichever preset was
# configured last, rather than .clangd pinning one of them.
if(CMAKE_EXPORT_COMPILE_COMMANDS AND NOT CMAKE_BINARY_DIR PATH_EQUAL "${CMAKE_SOURCE_DIR}/build")
    file(CREATE_LINK
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_SOURCE_DIR}/build/compile_commands.json"
        RESULT compileCommandsLinkResult
        COPY_ON_ERROR SYMBOLIC)

    if(NOT compileCommandsLinkResult EQUAL 0)
        message(WARNING "Could not link compile_commands.json into build/, clangd will not "
                        "find it: ${compileCommandsLinkResult}")
    endif()
endif()

# Set the start-up project to be the main executable CrispMain
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CrispMain)
