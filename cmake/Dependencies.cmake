include(FetchContent)

FetchContent_Declare(glm
    GIT_REPOSITORY "https://github.com/g-truc/glm.git"
    GIT_TAG "1.0.1"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(glfw
    GIT_REPOSITORY "https://github.com/glfw/glfw.git"
    GIT_TAG "3.4"
    GIT_SHALLOW TRUE
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(json
    GIT_REPOSITORY "https://github.com/nlohmann/json.git"
    GIT_TAG "v3.12.0"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(unordered_dense
    GIT_REPOSITORY "https://github.com/martinus/unordered_dense.git"
    GIT_TAG "v4.5.0"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(unordered_dense)

FetchContent_Declare(fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
    GIT_TAG "11.2.0"
    GIT_SHALLOW TRUE
)
set(FMT_OS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(spdlog
    GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
    GIT_TAG "v1.15.3"
    GIT_SHALLOW TRUE
)
set(SPDLOG_FMT_EXTERNAL_HO ON  CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE   OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

if(CRISP_BUILD_TESTS)
    FetchContent_Declare(googletest
        GIT_REPOSITORY "https://github.com/google/googletest.git"
        GIT_TAG "v1.17.0"
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

if(CRISP_BUILD_BENCHMARKS)
    FetchContent_Declare(benchmark
        GIT_REPOSITORY "https://github.com/google/benchmark.git"
        GIT_TAG "v1.9.4"
        GIT_SHALLOW TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_INSTALL_DOCS   OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()

FetchContent_Declare(tinygltf
    GIT_REPOSITORY "https://github.com/syoyo/tinygltf.git"
    GIT_TAG "v2.9.6"
    GIT_SHALLOW TRUE
)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_HEADER_ONLY          ON  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinygltf)

FetchContent_Declare(MPMCQueue
    GIT_REPOSITORY "https://github.com/rigtorp/MPMCQueue.git"
    GIT_TAG "b9808ede08f26fa9df4df4e081d19cace8f6c6ea"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(MPMCQueue)

# oneTBB must be built as SHARED (it provides TBB::tbb as an import target with a DLL).
# Save and restore BUILD_SHARED_LIBS to avoid affecting any other FetchContent targets.
FetchContent_Declare(onetbb
    GIT_REPOSITORY "https://github.com/oneapi-src/oneTBB.git"
    GIT_TAG "v2022.2.0"
    GIT_SHALLOW TRUE
)
set(TBB_TEST OFF CACHE BOOL "" FORCE)
set(_saved_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS ON)
FetchContent_MakeAvailable(onetbb)
set(BUILD_SHARED_LIBS ${_saved_BUILD_SHARED_LIBS})
unset(_saved_BUILD_SHARED_LIBS)

FetchContent_Declare(embree
    GIT_REPOSITORY "https://github.com/embree/embree.git"
    GIT_TAG "v4.4.0"
    GIT_SHALLOW TRUE
)
set(EMBREE_ISPC_SUPPORT OFF CACHE BOOL "" FORCE)
set(EMBREE_TUTORIALS    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(embree)

FetchContent_Declare(vulkan
    GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Headers.git"
    GIT_TAG "v1.4.326"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(vulkan)

FetchContent_Declare(volk
    GIT_REPOSITORY "https://github.com/zeux/volk.git"
    GIT_TAG "1.4.304"
    GIT_SHALLOW TRUE
)
set(VOLK_HEADERS_ONLY    ON  CACHE BOOL "" FORCE)
set(VOLK_PULL_IN_VULKAN  OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(volk)

FetchContent_Declare(VulkanMemoryAllocator
    GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git"
    GIT_TAG "v3.3.0"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

FetchContent_Declare(SPIRV-Reflect
    GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Reflect.git"
    GIT_TAG "vulkan-sdk-1.4.321.0"
    GIT_SHALLOW TRUE
)
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES   OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SPIRV-Reflect)

FetchContent_Declare(GSL
    GIT_REPOSITORY "https://github.com/microsoft/GSL"
    GIT_TAG "v4.1.0"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(GSL)

FetchContent_Declare(meshoptimizer
    GIT_REPOSITORY "https://github.com/zeux/meshoptimizer"
    GIT_TAG "v0.23"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(meshoptimizer)

add_subdirectory(Externals/imgui)
add_subdirectory(Externals/stb)
add_subdirectory(Externals/tinyexr)
add_subdirectory(Externals/rapidxml)
add_subdirectory(Externals/freetype)
