cmake_minimum_required(VERSION 3.21)

project(Crisp VERSION 0.1.0 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enables C++23 and high amount of warnings for a C++ target.
function(enable_default_cpp_compile_options targetName optionType)
    set_target_properties(${targetName} PROPERTIES CXX_STANDARD 23 LINKER_LANGUAGE CXX)
    if(MSVC)
      target_compile_options(${targetName} ${optionType} /MP /JMC /Zi /W4 /WX /Zc:preprocessor)
    else()
      target_compile_options(${targetName} ${optionType} -W -Wall -Werror)
    endif()
endfunction()

# Creates a static library with C++ 20 standard support.
macro(add_cpp_static_library targetName)
    add_library(${targetName} STATIC ${ARGN})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_include_directories(${targetName} PUBLIC ${CRISP_INCLUDE_DIR})
endmacro()

# Creates a shared library with C++ 20 standard support.
macro(add_cpp_shared_library targetName)
    add_library(${targetName} SHARED ${ARGN})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_include_directories(${targetName} PUBLIC ${CRISP_INCLUDE_DIR})
endmacro()

# Creates a header-only library with C++ 20 standard support.
macro(add_cpp_interface_library targetName)
    add_library(${targetName} INTERFACE ${ARGN})
    enable_default_cpp_compile_options(${targetName} INTERFACE)
    target_include_directories(${targetName} INTERFACE ${CRISP_INCLUDE_DIR})
endmacro()

# Creates a binary with C++ 20 standard support.
macro(add_cpp_binary targetName)
    add_executable(${targetName} ${ARGN})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
endmacro()

# Creates a test with C++ 20 standard support.
macro(add_cpp_test targetName)
    add_executable(${targetName} ${ARGN})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_link_libraries(${targetName} PRIVATE GTest::gtest)
    target_link_libraries(${targetName} PRIVATE GTest::gtest_main)
endmacro()

# Copies a list of shared libraries into the designated target's directory. TO be used with DLL dependencies.
function(copy_shared_libs targetName)
    foreach(sharedLib ${ARGN})
        add_custom_command(TARGET ${targetName} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${sharedLib}> $<TARGET_FILE_DIR:${targetName}>)
    endforeach()
endfunction()

#----------------------------------
# Third-party libraries
#----------------------------------

include(FetchContent)

# Grab readily available third parties from external repositories.
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(robin_hood
    GIT_REPOSITORY https://github.com/martinus/robin-hood-hashing.git
    GIT_TAG        3.11.5
)
FetchContent_MakeAvailable(robin_hood)

FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        9.1.0
)
set(FMT_OS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.10.0
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
# set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(tinygltf
  GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
  GIT_TAG        v2.6.3
)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinygltf)

FetchContent_Declare(MPMCQueue
  GIT_REPOSITORY https://github.com/rigtorp/MPMCQueue.git
  GIT_TAG        v1.0
)
FetchContent_MakeAvailable(MPMCQueue)

# TODO: Configure to be dynamic.
FetchContent_Declare(onetbb
  GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
  GIT_TAG        v2021.6.0
)
set(TBB_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(onetbb)

FetchContent_Declare(embree
  GIT_REPOSITORY https://github.com/embree/embree.git
  GIT_TAG        v3.13.5
)
set(EMBREE_ISPC_SUPPORT OFF CACHE BOOL "" FORCE)
set(EMBREE_TUTORIALS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(embree)

FetchContent_Declare(vulkan
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
  GIT_TAG        v1.3.246
)
FetchContent_MakeAvailable(vulkan)

# FetchContent_Declare(tinyexr
#   GIT_REPOSITORY https://github.com/syoyo/tinyexr.git
#   GIT_TAG        v1.0.1
# )
# set(TINYEXR_BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
# FetchContent_MakeAvailable(tinyexr)

# List all used third-party libraries
add_subdirectory(Externals/lua)
add_subdirectory(Externals/imgui)
add_subdirectory(Externals/stb)
add_subdirectory(Externals/tinyexr)
add_subdirectory(Externals/rapidxml)
add_subdirectory(Externals/freetype)
add_subdirectory(Externals/gzip)
add_subdirectory(Externals/zlib)

add_subdirectory(Crisp)

add_subdirectory(Vesper)
add_subdirectory(Vesper/Test)

# Set the start-up project to be the main executable CrispMain
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CrispMain)