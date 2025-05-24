add_cpp_static_library(
    CrispVulkanHeader
    "VulkanHeader.cpp"
    "VulkanHeader.hpp"
    "VulkanChecks.cpp"
    "VulkanChecks.hpp"
)
target_link_libraries(
    CrispVulkanHeader
    PUBLIC Vulkan::Headers
    PUBLIC volk::volk_headers
    PUBLIC GPUOpen::VulkanMemoryAllocator
    PUBLIC Crisp::Checks
)

add_cpp_static_library(
    CrispVulkanInstance
    "VulkanInstance.cpp"
    "VulkanInstance.hpp"
)
target_link_libraries(
    CrispVulkanInstance
    PUBLIC Crisp::VulkanHeader
)

add_cpp_test(
    CrispVulkanInstanceTest
    "Test/VulkanInstanceTest.cpp"
)
target_link_libraries(
    CrispVulkanInstanceTest
    PRIVATE Crisp::VulkanInstance
    PRIVATE Crisp::ResultTestUtils
    PRIVATE Crisp::Window
    PRIVATE Crisp::ApplicationEnvironment
)

add_cpp_static_library(
    CrispVulkanPhysicalDevice
    "VulkanPhysicalDevice.cpp"
    "VulkanPhysicalDevice.hpp"
)
target_link_libraries(
    CrispVulkanPhysicalDevice
    PUBLIC Crisp::VulkanInstance
    PRIVATE Crisp::HashMap
)

add_cpp_test(
    CrispVulkanPhysicalDeviceTest
    "Test/VulkanPhysicalDeviceTest.cpp"
)
target_link_libraries(
    CrispVulkanPhysicalDeviceTest
    PRIVATE Crisp::VulkanPhysicalDevice
    PRIVATE Crisp::VulkanTestUtils
)

add_cpp_static_library(
    CrispVulkanDevice
    "VulkanDevice.cpp"
    "VulkanDevice.hpp"
    "VulkanResourceDeallocator.cpp"
    "VulkanResourceDeallocator.hpp"
    "VulkanQueue.cpp"
    "VulkanQueue.hpp"
    "VulkanQueueConfiguration.cpp"
    "VulkanQueueConfiguration.hpp"
    "VulkanDescriptorSetAllocator.cpp"
    "VulkanDescriptorSetAllocator.hpp"
)
target_link_libraries(
    CrispVulkanDevice
    PUBLIC Crisp::VulkanInstance
    PUBLIC Crisp::VulkanPhysicalDevice
    PUBLIC Crisp::VulkanPipelineCache
    PUBLIC Crisp::HashMap
)

add_cpp_test(
    CrispVulkanDeviceTest
    "Test/VulkanDeviceTest.cpp"
)
target_link_libraries(
    CrispVulkanDeviceTest
    PRIVATE Crisp::VulkanDevice
    PRIVATE Crisp::VulkanTestUtils
)

add_cpp_test(
    CrispVulkanResourceDeallocatorTest
    "Test/VulkanResourceDeallocatorTest.cpp"
)
target_link_libraries(
    CrispVulkanResourceDeallocatorTest
    PRIVATE Crisp::VulkanDevice
    PRIVATE Crisp::VulkanTestUtils
)

add_cpp_interface_library(
    CrispVulkanResource
    "VulkanResource.hpp"
)
target_link_libraries(
    CrispVulkanResource
    INTERFACE Crisp::VulkanDevice
    INTERFACE Crisp::RendererConfig
)

add_cpp_static_library(
    CrispVulkanSwapChain
    "VulkanSwapChain.cpp"
    "VulkanSwapChain.hpp"
)
target_link_libraries(
    CrispVulkanSwapChain
    PUBLIC Crisp::VulkanDevice
)

add_cpp_test(
    CrispVulkanSwapChainTest
    "Test/VulkanSwapChainTest.cpp"
)
target_link_libraries(
    CrispVulkanSwapChainTest
    PRIVATE Crisp::VulkanSwapChain
    PRIVATE Crisp::VulkanTestUtils
)

add_cpp_static_library(
    CrispVulkanBuffer
    "VulkanBuffer.cpp"
    "VulkanBuffer.hpp"
)
target_link_libraries(
    CrispVulkanBuffer
    PUBLIC Crisp::VulkanDevice
)

add_cpp_test(
    CrispVulkanBufferTest
    "Test/VulkanBufferTest.cpp"
)
target_link_libraries(
    CrispVulkanBufferTest
    PRIVATE Crisp::VulkanBuffer
    PRIVATE Crisp::VulkanTestUtils
)

add_cpp_static_library(
    CrispVulkanImage
    "VulkanImage.cpp"
    "VulkanImage.hpp"
)
target_link_libraries(
    CrispVulkanImage
    PUBLIC Crisp::VulkanDevice
)

add_cpp_test(
    CrispVulkanImageTest
    "Test/VulkanImageTest.cpp"
)
target_link_libraries(
    CrispVulkanImageTest
    PRIVATE Crisp::VulkanImage
    PRIVATE Crisp::VulkanTestUtils
)

add_cpp_static_library(
    CrispVulkanSampler
    "VulkanSampler.cpp"
    "VulkanSampler.hpp"
)
target_link_libraries(
    CrispVulkanSampler
    PUBLIC Crisp::VulkanDevice
)

add_cpp_static_library(
    CrispVulkanImageView
    "VulkanImageView.cpp"
    "VulkanImageView.hpp"
)
target_link_libraries(
    CrispVulkanImageView
    PUBLIC Crisp::VulkanImage
    PUBLIC Crisp::VulkanSampler
)

add_cpp_static_library(
    CrispVulkanPipelineLayout
    "VulkanPipelineLayout.cpp"
    "VulkanPipelineLayout.hpp"
)
target_link_libraries(
    CrispVulkanPipelineLayout
    PUBLIC Crisp::VulkanDevice
)

add_cpp_static_library(
    CrispVulkanDescriptorSet
    "VulkanDescriptorSet.cpp"
    "VulkanDescriptorSet.hpp"
)
target_link_libraries(
    CrispVulkanDescriptorSet
    PUBLIC Crisp::VulkanDevice
    PUBLIC Crisp::VulkanPipelineLayout
)

add_cpp_static_library(
    CrispVulkanPipelineCache
    "VulkanPipelineCache.cpp"
    "VulkanPipelineCache.hpp"
)
target_link_libraries(
    CrispVulkanPipelineCache
    PRIVATE Crisp::VulkanDevice
    PRIVATE Crisp::FileUtils
)

add_cpp_static_library(
    CrispVulkanPipeline
    "VulkanPipeline.cpp"
    "VulkanPipeline.hpp"
)
target_link_libraries(
    CrispVulkanPipeline
    PUBLIC Crisp::VulkanDevice
    PUBLIC Crisp::VulkanPipelineLayout
    PUBLIC Crisp::VulkanDescriptorSet
    PUBLIC Crisp::VulkanVertexLayout
)

add_cpp_static_library(
    CrispVulkanFramebuffer
    "VulkanFramebuffer.cpp"
    "VulkanFramebuffer.hpp"
)
target_link_libraries(
    CrispVulkanFramebuffer
    PUBLIC Crisp::VulkanDevice
)

add_cpp_static_library(
    CrispVulkanRenderPass
    "VulkanRenderPass.cpp"
    "VulkanRenderPass.hpp"
)
target_link_libraries(
    CrispVulkanRenderPass
    PUBLIC Crisp::VulkanDevice
    PUBLIC Crisp::VulkanImageView
    PUBLIC Crisp::VulkanFramebuffer
)

add_cpp_static_library(
    CrispVulkanCommandBuffer
    "VulkanCommandBuffer.cpp"
    "VulkanCommandBuffer.hpp"
    "VulkanCommandPool.cpp"
    "VulkanCommandPool.hpp"
)
target_link_libraries(
    CrispVulkanCommandBuffer
    PUBLIC Crisp::VulkanDevice
)

add_cpp_static_library(
    CrispVulkanAccelerationStructure
    "VulkanAccelerationStructure.cpp"
    "VulkanAccelerationStructure.hpp"
)
target_link_libraries(
    CrispVulkanAccelerationStructure
    PUBLIC Crisp::VulkanDevice
    PUBLIC Crisp::Math
)

add_cpp_interface_library(
    CrispVulkanTestUtils
    "Test/VulkanTest.hpp"
)
target_link_libraries(
    CrispVulkanTestUtils
    INTERFACE Crisp::Vulkan
    INTERFACE Crisp::Window
    INTERFACE Crisp::ApplicationEnvironment
    INTERFACE Crisp::ResultTestUtils
)