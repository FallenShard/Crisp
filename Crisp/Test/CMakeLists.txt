add_executable(CrispTest
    "CameraTests/CameraTest.cpp"
    "CameraTests/FreeCameraControllerTest.cpp"
    "RendererTests/PipelineLayoutBuilderTest.cpp"
    "ShadingLanguageTests/LexerTest.cpp"
    "VulkanTests/ScopeCommandBufferExecutor.hpp"
    "VulkanTests/VulkanBufferTest.cpp"
    "VulkanTests/VulkanContextTest.cpp"
    "VulkanTests/VulkanDeviceTest.cpp"
    "VulkanTests/VulkanMemoryAllocatorTest.cpp"
    "VulkanTests/VulkanMemoryHeapTest.cpp"
    "VulkanTests/VulkanPhysicalDeviceTest.cpp"
    "VulkanTests/VulkanSwapChainTest.cpp"
    "VulkanTests/VulkanTest.hpp"
    "VulkanTests/VulkanResourceDeallocatorTest.cpp"
    "VulkanTests/VulkanComputeTest.cpp" "AtmosphereShadingTests/TransmittanceLutTest.cpp")

set_target_properties(CrispTest PROPERTIES
    CXX_STANDARD 20)

target_include_directories(CrispTest PUBLIC ".")

target_link_libraries(CrispTest PRIVATE gtest)
target_link_libraries(CrispTest PRIVATE gtestMain)
target_link_libraries(CrispTest PRIVATE CrispVulkan CrispGlslUtils CrispCamera CrispRenderer CrispModels)
copy_shared_libs(CrispTest vulkan lua freetype zlib tbb tbbMalloc embree)

add_cpp_binary(CrispCoreTest
    "BitFlagsTest.cpp"
    "CommandLineParserTest.cpp"
    "DelegateTest.cpp"
    "EventTest.cpp"
    "GlmStructuredBindingsTest.cpp"
    "LuaConfigTest.cpp"
    "StringUtilsTest.cpp"
    "WavefrontObjTest.cpp")

target_link_libraries(CrispCoreTest PRIVATE gtest)
target_link_libraries(CrispCoreTest PRIVATE gtestMain)
target_link_libraries(CrispCoreTest PRIVATE CrispMeshIo CrispEvent CrispApplicationEnvironment CrispBitFlags CrispUtils CrispStringUtils)

add_custom_command(TARGET CrispCoreTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:lua> $<TARGET_FILE_DIR:CrispCoreTest>
)
