add_subdirectory(Rhi)

add_cpp_static_library(CrispVulkan
    "VulkanAccelerationStructure.cpp"
    "VulkanAccelerationStructure.hpp"
    "VulkanBuffer.cpp"
    "VulkanBuffer.hpp"
    "VulkanCommandBuffer.cpp"
    "VulkanCommandBuffer.hpp"
    "VulkanCommandPool.cpp"
    "VulkanCommandPool.hpp"
    "VulkanDebugUtils.cpp"
    "VulkanDebugUtils.hpp"
    "VulkanDescriptorSet.cpp"
    "VulkanDescriptorSet.hpp"
    "VulkanDescriptorSetAllocator.cpp"
    "VulkanDescriptorSetAllocator.hpp"
    "VulkanEnumToString.hpp"
    "VulkanEnumToString.cpp"
    "VulkanFormatTraits.hpp"
    "VulkanFramebuffer.cpp"
    "VulkanFramebuffer.hpp"
    "VulkanImage.cpp"
    "VulkanImage.hpp"
    "VulkanImageView.cpp"
    "VulkanImageView.hpp"
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
    PUBLIC Crisp::Math
    PUBLIC Crisp::VulkanInstance
    PUBLIC Crisp::VulkanPhysicalDevice
    PUBLIC Crisp::VulkanDevice
)

add_cpp_test(CrispVulkanBufferTest
    "Test/VulkanBufferTest.cpp")
target_link_libraries(CrispVulkanBufferTest PRIVATE Crisp::VulkanTestUtils)

add_cpp_test(CrispVulkanSwapChainTest
    "Test/VulkanSwapChainTest.cpp")
target_link_libraries(CrispVulkanSwapChainTest PRIVATE Crisp::VulkanTestUtils)

add_cpp_test(CrispVulkanResourceDeallocatorTest
    "Test/VulkanResourceDeallocatorTest.cpp")
target_link_libraries(CrispVulkanResourceDeallocatorTest PRIVATE Crisp::VulkanTestUtils)

add_cpp_test(CrispVulkanImageTest
    "Test/VulkanImageTest.cpp")
target_link_libraries(CrispVulkanImageTest PRIVATE Crisp::VulkanTestUtils)

add_cpp_test(CrispVulkanVertexLayoutTest
    "Test/VulkanVertexLayoutTest.cpp")
target_link_libraries(CrispVulkanVertexLayoutTest PRIVATE Crisp::VulkanTestUtils)