add_cpp_static_library(CrispVulkanHeader
    "VulkanHeader.cpp"
    "VulkanHeader.hpp"
)
target_link_libraries(CrispVulkanHeader
    PUBLIC Vulkan::Headers
    PUBLIC volk::volk_headers
    PRIVATE Crisp::Checks
)

add_cpp_static_library(CrispVulkan
    "VulkanAccelerationStructure.cpp"
    "VulkanAccelerationStructure.hpp"
    "VulkanBuffer.cpp"
    "VulkanBuffer.hpp"
    "VulkanChecks.cpp"
    "VulkanChecks.hpp"
    "VulkanCommandBuffer.cpp"
    "VulkanCommandBuffer.hpp"
    "VulkanCommandPool.cpp"
    "VulkanCommandPool.hpp"
    "VulkanContext.cpp"
    "VulkanContext.hpp"
    "VulkanDebugUtils.cpp"
    "VulkanDebugUtils.hpp"
    "VulkanDescriptorSet.cpp"
    "VulkanDescriptorSet.hpp"
    "VulkanDescriptorSetAllocator.cpp"
    "VulkanDescriptorSetAllocator.hpp"
    "VulkanDevice.cpp"
    "VulkanDevice.hpp"
    "VulkanEnumToString.hpp"
    "VulkanEnumToString.cpp"
    "VulkanFormatTraits.hpp"
    "VulkanFramebuffer.cpp"
    "VulkanFramebuffer.hpp"
    "VulkanImage.cpp"
    "VulkanImage.hpp"
    "VulkanImageView.cpp"
    "VulkanImageView.hpp"
    "VulkanMemoryAllocator.cpp"
    "VulkanMemoryAllocator.hpp"
    "VulkanMemoryHeap.cpp"
    "VulkanMemoryHeap.hpp"
    "VulkanPhysicalDevice.cpp"
    "VulkanPhysicalDevice.hpp"
    "VulkanPipeline.cpp"
    "VulkanPipeline.hpp"
    "VulkanPipelineLayout.cpp"
    "VulkanPipelineLayout.hpp"
    "VulkanQueue.cpp"
    "VulkanQueue.hpp"
    "VulkanQueueConfiguration.cpp"
    "VulkanQueueConfiguration.hpp"
    "VulkanRenderPass.cpp"
    "VulkanRenderPass.hpp"
    "VulkanResource.hpp"
    "VulkanResourceDeallocator.hpp"
    "VulkanResourceDeallocator.cpp"
    "VulkanSampler.cpp"
    "VulkanSampler.hpp"
    "VulkanSwapChain.cpp"
    "VulkanSwapChain.hpp"
    "VulkanVertexLayout.cpp"
    "VulkanVertexLayout.hpp"
)
target_link_libraries(CrispVulkan
    PUBLIC Crisp::BitFlags
    PUBLIC Crisp::RendererConfig
    PUBLIC Crisp::Result
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::Logger
    PUBLIC Crisp::Checks
    PUBLIC Crisp::VulkanHeader
    PUBLIC Crisp::Math
)

add_cpp_interface_library(CrispVulkanTestUtils
    "Test/VulkanTest.hpp")
target_link_libraries(CrispVulkanTestUtils
    INTERFACE Crisp::Vulkan CrispWindow CrispApplicationEnvironment CrispResultTestUtils)

add_cpp_test(CrispVulkanBufferTest
    "Test/VulkanBufferTest.cpp")
target_link_libraries(CrispVulkanBufferTest PRIVATE Crisp::VulkanTestUtils)

add_cpp_test(CrispVulkanContextTest
    "Test/VulkanContextTest.cpp")
target_link_libraries(CrispVulkanContextTest PRIVATE Crisp::VulkanTestUtils)

add_cpp_test(CrispVulkanDeviceTest
    "Test/VulkanDeviceTest.cpp")
target_link_libraries(CrispVulkanDeviceTest PRIVATE Crisp::VulkanTestUtils)

add_cpp_test(CrispVulkanMemoryAllocatorTest
    "Test/VulkanMemoryAllocatorTest.cpp")
target_link_libraries(CrispVulkanMemoryAllocatorTest PRIVATE Crisp::VulkanTestUtils)

add_cpp_test(CrispVulkanMemoryHeapTest
    "Test/VulkanMemoryHeapTest.cpp")
target_link_libraries(CrispVulkanMemoryHeapTest PRIVATE Crisp::VulkanTestUtils)

add_cpp_test(CrispVulkanPhysicalDeviceTest
    "Test/VulkanPhysicalDeviceTest.cpp")
target_link_libraries(CrispVulkanPhysicalDeviceTest PRIVATE Crisp::VulkanTestUtils)

add_cpp_test(CrispVulkanSwapChainTest
    "Test/VulkanSwapChainTest.cpp")
target_link_libraries(CrispVulkanSwapChainTest PRIVATE Crisp::VulkanTestUtils)

add_cpp_test(CrispVulkanResourceDeallocatorTest
    "Test/VulkanResourceDeallocatorTest.cpp")
target_link_libraries(CrispVulkanResourceDeallocatorTest PRIVATE Crisp::VulkanTestUtils)
