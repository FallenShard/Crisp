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
    PUBLIC CrispVulkanHeader)

add_cpp_static_library(CrispVulkanHeader
    "VulkanHeader.cpp"
    "VulkanHeader.hpp"
)
target_link_libraries(CrispVulkanHeader
    PUBLIC vulkan
    PRIVATE CrispChecks)

add_cpp_interface_library(CrispVulkanTestUtils
    "Test/ScopeCommandBufferExecutor.hpp"
    "Test/VulkanTest.hpp")
target_link_libraries(CrispVulkanTestUtils INTERFACE CrispVulkan CrispWindow CrispApplicationEnvironment)

add_cpp_test(CrispVulkanTest
    "Test/VulkanBufferTest.cpp"
    "Test/VulkanContextTest.cpp"
    "Test/VulkanDeviceTest.cpp"
    "Test/VulkanMemoryAllocatorTest.cpp"
    "Test/VulkanMemoryHeapTest.cpp"
    "Test/VulkanPhysicalDeviceTest.cpp"
    "Test/VulkanSwapChainTest.cpp"
    "Test/VulkanResourceDeallocatorTest.cpp"
    "Test/VulkanComputeTest.cpp")
target_link_libraries(CrispVulkanTest PRIVATE CrispVulkanTestUtils CrispVulkan CrispWindow CrispApplicationEnvironment)
copy_shared_libs(CrispVulkanTest lua)