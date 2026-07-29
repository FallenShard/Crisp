include(FetchContent)

FetchContent_Declare(glm
    GIT_REPOSITORY "https://github.com/g-truc/glm.git"
    GIT_TAG "1.0.3"
    GIT_SHALLOW TRUE
)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(glfw
    GIT_REPOSITORY "https://github.com/glfw/glfw.git"
    GIT_TAG "3.4"
    GIT_SHALLOW TRUE
)

# These stay CACHE/FORCE deliberately: CMP0077 (option() defers to a normal variable) is
# resolved by each dependency's own cmake_minimum_required, and most of ours declare < 3.13,
# so they get the OLD behaviour where option() overwrites a plain set().
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(json
    GIT_REPOSITORY "https://github.com/nlohmann/json.git"
    GIT_TAG "v3.12.0"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(unordered_dense
    GIT_REPOSITORY "https://github.com/martinus/unordered_dense.git"
    GIT_TAG "v4.8.1"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(unordered_dense)

FetchContent_Declare(fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
    GIT_TAG "12.1.0"
    GIT_SHALLOW TRUE
)
set(FMT_OS OFF CACHE BOOL "" FORCE)
set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_DOC OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(spdlog
    GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
    GIT_TAG "v1.17.0"
    GIT_SHALLOW TRUE
)
set(SPDLOG_FMT_EXTERNAL_HO ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

if(CRISP_BUILD_TESTS)
    FetchContent_Declare(googletest
        GIT_REPOSITORY "https://github.com/google/googletest.git"
        GIT_TAG "v1.17.0"
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

if(CRISP_BUILD_TESTS)
    FetchContent_Declare(glslang
        GIT_REPOSITORY "https://github.com/KhronosGroup/glslang.git"
        GIT_TAG "16.4.0"
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
    )
    block()
    set(BUILD_EXTERNAL OFF)
    set(GLSLANG_TESTS OFF)
    set(GLSLANG_ENABLE_INSTALL OFF)
    set(ENABLE_GLSLANG_BINARIES ON)
    set(ENABLE_GLSLANG_JS OFF)
    set(ENABLE_SPIRV ON)
    set(ENABLE_OPT OFF)
    set(ENABLE_HLSL OFF)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")

    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "")
    endif()

    FetchContent_MakeAvailable(glslang)
    endblock()
    set_target_properties(glslang-standalone PROPERTIES FOLDER "ThirdParty/Tools")
endif()

if(CRISP_BUILD_BENCHMARKS)
    FetchContent_Declare(benchmark
        GIT_REPOSITORY "https://github.com/google/benchmark.git"
        GIT_TAG "v1.9.5"
        GIT_SHALLOW TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)

    if(MSVC)
        target_compile_options(benchmark PRIVATE /EHsc)
        target_compile_options(benchmark_main PRIVATE /EHsc)
    endif()
endif()

FetchContent_Declare(tinygltf
    GIT_REPOSITORY "https://github.com/syoyo/tinygltf.git"
    GIT_TAG "v3.0.0"
    GIT_SHALLOW TRUE
)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinygltf)

FetchContent_Declare(MPMCQueue
    GIT_REPOSITORY "https://github.com/rigtorp/MPMCQueue.git"
    GIT_TAG "b9808ede08f26fa9df4df4e081d19cace8f6c6ea"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(MPMCQueue)

FetchContent_Declare(onetbb
    GIT_REPOSITORY "https://github.com/oneapi-src/oneTBB.git"
    GIT_TAG "v2023.1.0"
    GIT_SHALLOW TRUE
)
block()
set(BUILD_SHARED_LIBS ON)

# oneTBB declares cmake_minimum_required 3.5, so its option() calls run with CMP0077 OLD
# and would ignore plain variables -- these have to go through the cache.
set(TBB_TEST OFF CACHE BOOL "" FORCE)
set(TBB_STRICT OFF CACHE BOOL "" FORCE) # Defaults ON: a new MSVC warning in TBB fails our build.
set(TBB_INSTALL OFF CACHE BOOL "" FORCE)
set(TBBMALLOC_BUILD OFF CACHE BOOL "" FORCE)
set(TBB_VERIFY_DEPENDENCY_SIGNATURE ON CACHE BOOL "" FORCE)
set(TBB_DISABLE_HWLOC_AUTOMATIC_SEARCH ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(onetbb)
endblock()

FetchContent_Declare(embree
    GIT_REPOSITORY "https://github.com/embree/embree.git"
    GIT_TAG "v4.4.1"
    GIT_SHALLOW TRUE
)
block()

# Embree uses the parent project's generic BUILD_TESTING option. Keep Crisp's
# CTest support enabled without registering Embree's test suite.
set(BUILD_TESTING OFF)
set(EMBREE_ISPC_SUPPORT OFF CACHE BOOL "" FORCE)
set(EMBREE_TUTORIALS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(embree)
endblock()

FetchContent_Declare(vulkan
    GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Headers.git"
    GIT_TAG "v1.4.357"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(vulkan)

FetchContent_Declare(volk
    GIT_REPOSITORY "https://github.com/zeux/volk.git"
    GIT_TAG "1.4.350"
    GIT_SHALLOW TRUE
)
set(VOLK_HEADERS_ONLY ON CACHE BOOL "" FORCE)
set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(volk)

FetchContent_Declare(VulkanMemoryAllocator
    GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git"
    GIT_TAG "v3.4.0"
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

FetchContent_Declare(SPIRV-Reflect
    GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Reflect.git"
    GIT_TAG "vulkan-sdk-1.4.350.1"
    GIT_SHALLOW TRUE
)
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SPIRV-Reflect)

FetchContent_Declare(GSL
    GIT_REPOSITORY "https://github.com/microsoft/GSL"
    GIT_TAG "v4.2.2"
    GIT_SHALLOW TRUE
)
set(GSL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(GSL)

FetchContent_Declare(meshoptimizer
    GIT_REPOSITORY "https://github.com/zeux/meshoptimizer"
    GIT_TAG "v1.2"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(meshoptimizer)

# SYSTEM: these are all third-party sources, so their headers should not be held to our /W4.
add_subdirectory(Externals/imgui SYSTEM)
add_subdirectory(Externals/stb SYSTEM)
add_subdirectory(Externals/tinyexr SYSTEM)
add_subdirectory(Externals/rapidxml SYSTEM)
add_subdirectory(Externals/freetype SYSTEM)
