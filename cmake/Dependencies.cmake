include(FetchContent)

block()
set(CMAKE_FOLDER "ThirdParty")

FetchContent_Declare(glm
    GIT_REPOSITORY "https://github.com/g-truc/glm.git"
    GIT_TAG "1.0.3"
    GIT_SHALLOW TRUE
)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(glfw
    GIT_REPOSITORY "https://github.com/glfw/glfw.git"
    GIT_TAG "3.5.1"
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
    GIT_TAG "v5.0.1"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(unordered_dense)

FetchContent_Declare(fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
    GIT_TAG "12.2.0"
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
        GIT_TAG "v1.18.0"
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

FetchContent_Declare(glslang
    GIT_REPOSITORY "https://github.com/KhronosGroup/glslang.git"
    GIT_TAG "16.6.0"
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
block()
set(BUILD_EXTERNAL OFF)
set(GLSLANG_TESTS OFF)
set(GLSLANG_ENABLE_INSTALL OFF)
set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_GLSLANG_JS OFF)
set(ENABLE_SPIRV ON)
set(ENABLE_OPT OFF)
set(ENABLE_HLSL OFF)

FetchContent_MakeAvailable(glslang)
endblock()

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

FetchContent_Declare(VulkanHeaders
    GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Headers.git"
    GIT_TAG "vulkan-sdk-1.4.357.0"
    GIT_SHALLOW TRUE
    OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(VulkanHeaders)

FetchContent_Declare(volk
    GIT_REPOSITORY "https://github.com/zeux/volk.git"
    GIT_TAG "1.4.350"
    GIT_SHALLOW TRUE
)
set(VOLK_HEADERS_ONLY ON CACHE BOOL "" FORCE)
set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(volk)

FetchContent_Declare(SPIRV-Headers
    GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Headers.git"
    GIT_TAG "vulkan-sdk-1.4.357.0"
    GIT_SHALLOW TRUE
    OVERRIDE_FIND_PACKAGE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(SPIRV-Headers)
set(SPIRV-Headers_SOURCE_DIR "${spirv-headers_SOURCE_DIR}")

FetchContent_Declare(SPIRV-Tools-opt
    GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Tools.git"
    GIT_TAG "vulkan-sdk-1.4.357.0"
    GIT_SHALLOW TRUE
    OVERRIDE_FIND_PACKAGE
    EXCLUDE_FROM_ALL
)
set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SPIRV_WERROR OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SPIRV-Tools-opt)

FetchContent_Declare(VulkanUtilityLibraries
    GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Utility-Libraries.git"
    GIT_TAG "vulkan-sdk-1.4.357.0"
    GIT_SHALLOW TRUE
    OVERRIDE_FIND_PACKAGE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(VulkanUtilityLibraries)

set(BUILD_WERROR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(Vulkan-ValidationLayers
    GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-ValidationLayers.git"
    GIT_TAG "vulkan-sdk-1.4.357.0"
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
set(VVL_ENABLE_ASAN OFF CACHE BOOL "" FORCE)
set(VVL_ENABLE_UBSAN OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Vulkan-ValidationLayers)
set_target_properties(vvl PROPERTIES DEBUG_POSTFIX "")

FetchContent_Declare(Vulkan-ExtensionLayer
    GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-ExtensionLayer.git"
    GIT_TAG "vulkan-sdk-1.4.357.0"
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(Vulkan-ExtensionLayer)
set_target_properties(VkLayer_khronos_synchronization2 PROPERTIES DEBUG_POSTFIX "")

FetchContent_Declare(VulkanMemoryAllocator
    GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git"
    GIT_TAG "v3.4.0"
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

FetchContent_Declare(SPIRV-Reflect
    GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Reflect.git"
    GIT_TAG "vulkan-sdk-1.4.357.0"
    GIT_SHALLOW TRUE
)
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SPIRV-Reflect)

FetchContent_Declare(GSL
    GIT_REPOSITORY "https://github.com/microsoft/GSL"
    GIT_TAG "v5.0.0"
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

FetchContent_Declare(imgui
    GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
    GIT_TAG "v1.92.9b"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_cpp_static_library(ImGui
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)
target_include_directories(ImGui SYSTEM PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
)
target_compile_definitions(ImGui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
target_link_libraries(ImGui
    PUBLIC glfw
    PUBLIC Vulkan::Headers
)

FetchContent_Declare(stb
    GIT_REPOSITORY "https://github.com/nothings/stb.git"
    GIT_TAG "31c1ad37456438565541f4919958214b6e762fb4"
)
FetchContent_MakeAvailable(stb)

add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE "${stb_SOURCE_DIR}")
set_target_properties(stb PROPERTIES FOLDER "ThirdParty")

FetchContent_Declare(wuffs
    GIT_REPOSITORY "https://github.com/google/wuffs.git"
    GIT_TAG "v0.3.5"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(wuffs)

add_library(Wuffs INTERFACE)
target_include_directories(Wuffs SYSTEM INTERFACE "${wuffs_SOURCE_DIR}/release/c")
set_target_properties(Wuffs PROPERTIES FOLDER "ThirdParty")

FetchContent_Declare(OpenEXR
    GIT_REPOSITORY "https://github.com/AcademySoftwareFoundation/openexr.git"
    GIT_TAG "v3.4.15"
    GIT_SHALLOW TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
)
block()
set(OPENEXR_INSTALL OFF)
set(OPENEXR_INSTALL_PKG_CONFIG OFF)
set(OPENEXR_BUILD_TOOLS OFF)
set(OPENEXR_INSTALL_TOOLS OFF)
set(OPENEXR_INSTALL_DEVELOPER_TOOLS OFF)
set(OPENEXR_BUILD_EXAMPLES OFF)
set(OPENEXR_BUILD_PYTHON OFF)
set(OPENEXR_BUILD_OSS_FUZZ OFF)
set(OPENEXR_TEST_LIBRARIES OFF)
set(OPENEXR_TEST_TOOLS OFF)
set(OPENEXR_TEST_PYTHON OFF)
set(OPENEXR_FORCE_INTERNAL_IMATH ON)
set(OPENEXR_FORCE_INTERNAL_DEFLATE ON)
set(OPENEXR_FORCE_INTERNAL_OPENJPH ON)
FetchContent_MakeAvailable(OpenEXR)
endblock()

endblock()
