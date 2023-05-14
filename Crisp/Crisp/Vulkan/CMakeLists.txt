add_cpp_static_library(CrispVulkan
    "DescriptorSetAllocator.cpp"
    "DescriptorSetAllocator.hpp"
    "VulkanAccelerationStructure.cpp"
    "VulkanAccelerationStructure.hpp"
    "VulkanGetDeviceProc.hpp"
    "VulkanBuffer.cpp"
    "VulkanBuffer.hpp"
    "VulkanBufferView.hpp"
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
    "VulkanDevice.cpp"
    "VulkanDevice.hpp"
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
    "VulkanEnumToString.hpp"
    "VulkanEnumToString.cpp"
)
target_link_libraries(CrispVulkan
    PUBLIC CrispBitFlags
    PUBLIC CrispRendererConfig
    PUBLIC CrispResult
    PUBLIC CrispHashMap
    PRIVATE CrispLogger
    PUBLIC CrispUtils
    PUBLIC CrispChecks
    PUBLIC CrispVertexLayout
    PUBLIC CrispVulkanHeader)

add_cpp_static_library(CrispVulkanHeader
    "VulkanHeader.cpp"
    "VulkanHeader.hpp"
)
target_link_libraries(CrispVulkanHeader
    PUBLIC Vulkan::Headers
    PRIVATE CrispChecks)

add_cpp_interface_library(CrispVulkanTestUtils
    "Test/ScopeCommandBufferExecutor.hpp"
    "Test/VulkanTest.hpp")
target_link_libraries(CrispVulkanTestUtils INTERFACE CrispVulkan CrispWindow CrispApplicationEnvironment)

add_cpp_test(CrispVulkanBufferTest
    "Test/VulkanBufferTest.cpp")
target_link_libraries(CrispVulkanBufferTest PRIVATE CrispVulkanTestUtils)

add_cpp_test(CrispVulkanContextTest
    "Test/VulkanContextTest.cpp")
target_link_libraries(CrispVulkanContextTest PRIVATE CrispVulkanTestUtils)

add_cpp_test(CrispVulkanDeviceTest
    "Test/VulkanDeviceTest.cpp")
target_link_libraries(CrispVulkanDeviceTest PRIVATE CrispVulkanTestUtils)

add_cpp_test(CrispVulkanMemoryAllocatorTest
    "Test/VulkanMemoryAllocatorTest.cpp")
target_link_libraries(CrispVulkanMemoryAllocatorTest PRIVATE CrispVulkanTestUtils)

add_cpp_test(CrispVulkanMemoryHeapTest
    "Test/VulkanMemoryHeapTest.cpp")
target_link_libraries(CrispVulkanMemoryHeapTest PRIVATE CrispVulkanTestUtils)

add_cpp_test(CrispVulkanPhysicalDeviceTest
    "Test/VulkanPhysicalDeviceTest.cpp")
target_link_libraries(CrispVulkanPhysicalDeviceTest PRIVATE CrispVulkanTestUtils)

add_cpp_test(CrispVulkanSwapChainTest
    "Test/VulkanSwapChainTest.cpp")
target_link_libraries(CrispVulkanSwapChainTest PRIVATE CrispVulkanTestUtils)

add_cpp_test(CrispVulkanResourceDeallocatorTest
    "Test/VulkanResourceDeallocatorTest.cpp")
target_link_libraries(CrispVulkanResourceDeallocatorTest PRIVATE CrispVulkanTestUtils)

add_cpp_test(CrispVulkanComputeTest
    "Test/VulkanComputeTest.cpp")
target_link_libraries(CrispVulkanComputeTest PRIVATE CrispVulkanTestUtils)