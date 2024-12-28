cmake_minimum_required(VERSION 3.28)
project(Crisp VERSION 0.1.0 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

enable_testing()

# Enables C++23 and high amount of warnings for a C++ target.
function(enable_default_cpp_compile_options targetName optionType)
    # set_target_properties(${targetName} PROPERTIES CXX_STANDARD 23 LINKER_LANGUAGE CXX)
    set_target_properties(${targetName} PROPERTIES LANGUAGE CXX LINKER_LANGUAGE CXX)
    target_compile_features(${targetName} ${optionType} cxx_std_23)

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC") # no Clang no Intel
        target_compile_options(${targetName} ${optionType} /MP /JMC /Zi /W4 /Zc:preprocessor)
    else()
        target_compile_options(${targetName} ${optionType} -W -Wall)
    endif()
endfunction()

function(add_crisp_alias_target targetName)
    string(SUBSTRING "${targetName}" 0 5 targetPrefix)

    if(targetPrefix STREQUAL "Crisp")
        string(SUBSTRING "${targetName}" 5 -1 subTargetName)
        add_library("Crisp::${subTargetName}" ALIAS ${targetName})
    endif()
endfunction()

# Creates a C++ static library target.
macro(add_cpp_static_library targetName)
    add_library(${targetName} STATIC ${ARGN})
    add_crisp_alias_target(${targetName})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_include_directories(${targetName} PUBLIC ${CRISP_INCLUDE_DIR})
endmacro()

# Creates a C++ shared library target.
macro(add_cpp_shared_library targetName)
    add_library(${targetName} SHARED ${ARGN})
    add_crisp_alias_target(${targetName})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_include_directories(${targetName} PUBLIC ${CRISP_INCLUDE_DIR})
endmacro()

# Creates a C++ header-only library target.
macro(add_cpp_interface_library targetName)
    add_library(${targetName} INTERFACE ${ARGN})
    add_crisp_alias_target(${targetName})
    enable_default_cpp_compile_options(${targetName} INTERFACE)
    target_include_directories(${targetName} INTERFACE ${CRISP_INCLUDE_DIR})
endmacro()

# Creates a C++ binary target.
macro(add_cpp_binary targetName)
    add_executable(${targetName} ${ARGN})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
endmacro()

# Creates a C++ binary test target.
macro(add_cpp_test targetName)
    add_executable(${targetName} ${ARGN})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_link_libraries(${targetName} PRIVATE GTest::gmock)
    target_link_libraries(${targetName} PRIVATE GTest::gmock_main)

    add_test(NAME ${targetName} COMMAND $<TARGET_FILE:${targetName}>)
endmacro()

# Creates a C++ binary benchmark target.
macro(add_cpp_benchmark targetName)
    add_executable(${targetName} ${ARGN})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_link_libraries(${targetName} PRIVATE benchmark::benchmark)
endmacro()

# Copies a list of shared libraries into the designated target's directory. TO be used with DLL dependencies.
function(copy_shared_libs targetName)
    foreach(sharedLib ${ARGN})
        add_custom_command(TARGET ${targetName} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${sharedLib}> $<TARGET_FILE_DIR:${targetName}>)
    endforeach()
endfunction()

# ----------------------------------
# Third-party libraries
# ----------------------------------
include(FetchContent)

# Grab readily available third parties from external repositories.
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(robin_hood
    GIT_REPOSITORY https://github.com/martinus/robin-hood-hashing.git
    GIT_TAG 3.11.5
)
FetchContent_MakeAvailable(robin_hood)

FetchContent_Declare(unordered_dense
    GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
    GIT_TAG v4.4.0
)
FetchContent_MakeAvailable(unordered_dense)

FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
)
set(FMT_OS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
set(SPDLOG_FMT_EXTERNAL_HO ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
# set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

FetchContent_Declare(tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v2.8.21
)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinygltf)

FetchContent_Declare(MPMCQueue
    GIT_REPOSITORY https://github.com/rigtorp/MPMCQueue.git
    GIT_TAG v1.0
)
FetchContent_MakeAvailable(MPMCQueue)

# TODO: Configure to be dynamic.
FetchContent_Declare(onetbb
    GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
    GIT_TAG v2021.11.0
)
set(TBB_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS ON)
FetchContent_MakeAvailable(onetbb)
set(BUILD_SHARED_LIBS OFF)

FetchContent_Declare(embree
    GIT_REPOSITORY https://github.com/embree/embree.git
    GIT_TAG v4.3.1
)
set(EMBREE_ISPC_SUPPORT OFF CACHE BOOL "" FORCE)
set(EMBREE_TUTORIALS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(embree)

FetchContent_Declare(vulkan
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG v1.4.304
)
FetchContent_MakeAvailable(vulkan)

FetchContent_Declare(volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG 1.3.295
)
set(VOLK_HEADERS_ONLY ON CACHE BOOL "" FORCE)
set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(volk)

FetchContent_Declare(SPIRV-Reflect
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
    GIT_TAG vulkan-sdk-1.3.296.0
)
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SPIRV-Reflect)

FetchContent_Declare(GSL
    GIT_REPOSITORY "https://github.com/microsoft/GSL"
    GIT_TAG "v4.1.0"
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(GSL)

# List all used third-party libraries
add_subdirectory(Externals/imgui)
add_subdirectory(Externals/stb)
add_subdirectory(Externals/tinyexr)
add_subdirectory(Externals/rapidxml)
add_subdirectory(Externals/freetype)
add_subdirectory(Crisp)

# Set the start-up project to be the main executable CrispMain
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CrispMain)