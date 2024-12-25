add_subdirectory(Rhi)

add_cpp_static_library(CrispVulkan
    "VulkanAccelerationStructure.cpp"
    "VulkanAccelerationStructure.hpp"
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
    "VulkanImageView.cpp"
    "VulkanImageView.hpp"
    "VulkanPipeline.cpp"
    "VulkanPipeline.hpp"
    "VulkanPipelineLayout.cpp"
    "VulkanPipelineLayout.hpp"
    "VulkanRenderPass.cpp"
    "VulkanRenderPass.hpp"
    "VulkanSampler.cpp"
    "VulkanSampler.hpp"
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
    PUBLIC Crisp::VulkanResource
    PUBLIC Crisp::VulkanSwapChain
    PUBLIC Crisp::VulkanBuffer
    PUBLIC Crisp::VulkanImage
)

add_cpp_test(CrispVulkanVertexLayoutTest
    "Test/VulkanVertexLayoutTest.cpp")
target_link_libraries(CrispVulkanVertexLayoutTest PRIVATE Crisp::VulkanTestUtils)