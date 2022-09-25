add_cpp_test(CrispTest
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
    "VulkanTests/VulkanComputeTest.cpp"
    "AtmosphereShadingTests/TransmittanceLutTest.cpp")
target_link_libraries(CrispTest PRIVATE CrispVulkan CrispGlslUtils CrispCamera CrispRenderer CrispModels)
copy_shared_libs(CrispTest vulkan lua freetype zlib)

add_cpp_test(CrispCoreTest
    "BitFlagsTest.cpp"
    "CommandLineParserTest.cpp"
    "DelegateTest.cpp"
    "EventTest.cpp"
    "GlmStructuredBindingsTest.cpp"
    "LuaConfigTest.cpp"
    "StringUtilsTest.cpp"
    "WavefrontObjTest.cpp")
target_link_libraries(CrispCoreTest PRIVATE CrispFormat CrispLogger CrispMeshIo CrispEvent CrispApplicationEnvironment CrispBitFlags CrispUtils CrispStringUtils)
copy_shared_libs(CrispCoreTest lua)

