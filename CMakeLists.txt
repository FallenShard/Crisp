cmake_minimum_required(VERSION 3.20)

project(Crisp VERSION 0.1.0 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/MP /JMC /Zi /W4 /WX /Zc:preprocessor)
else()
  add_compile_options(-W -Wall -Werror)
endif()

# Creates a static library with C++ 20 standard support.
function(add_cpp_static_library targetName)
    add_library(${targetName} STATIC ${ARGN})
    set_target_properties(${targetName} PROPERTIES CXX_STANDARD 20 LINKER_LANGUAGE CXX)
    target_include_directories(${targetName} PUBLIC ${CRISP_INCLUDE_DIR})
endfunction()

# Creates a header-only library with C++ 20 standard support.
function(add_cpp_interface_library targetName)
    add_library(${targetName} INTERFACE ${ARGN})
    set_target_properties(${targetName} PROPERTIES CXX_STANDARD 20 LINKER_LANGUAGE CXX)
    target_include_directories(${targetName} INTERFACE ${CRISP_INCLUDE_DIR})
endfunction()

# Creates a binary with C++ 20 standard support.
function(add_cpp_binary targetName)
    add_executable(${targetName} ${ARGN})
    set_target_properties(${targetName} PROPERTIES CXX_STANDARD 20 LINKER_LANGUAGE CXX)
endfunction()

# Copies a list of shared libraries into the designated target's directory.
function(copy_shared_libs targetName)
    foreach(sharedLib ${ARGN})
        add_custom_command(TARGET ${targetName} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${sharedLib}> $<TARGET_FILE_DIR:${targetName}>)
    endforeach()
endfunction()


# List all used third-party libraries
add_subdirectory(Externals/lua)
add_subdirectory(Externals/stb)
add_subdirectory(Externals/glfw)
add_subdirectory(Externals/vulkan)
add_subdirectory(Externals/glm)
add_subdirectory(Externals/fmt)
add_subdirectory(Externals/tinyexr)
add_subdirectory(Externals/embree3)
add_subdirectory(Externals/tbb)
add_subdirectory(Externals/rapidxml)
add_subdirectory(Externals/freetype)
add_subdirectory(Externals/gzip)
add_subdirectory(Externals/zlib)
add_subdirectory(Externals/gtest)
add_subdirectory(Externals/spdlog)
add_subdirectory(Externals/MPMCQueue)
add_subdirectory(Externals/robin_hood)
add_subdirectory(Externals/tinygltf)
add_subdirectory(Externals/json)

add_subdirectory(Crisp)

add_subdirectory(Vesper)
add_subdirectory(Vesper/Test)

# Set the start-up project to be the main executable CrispMain
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CrispMain)