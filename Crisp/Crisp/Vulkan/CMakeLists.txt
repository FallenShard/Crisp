add_subdirectory(Rhi)

add_cpp_static_library(CrispVulkan
    "VulkanEnumToString.hpp"
    "VulkanEnumToString.cpp"
)
target_link_libraries(CrispVulkan
    PUBLIC Crisp::VulkanInstance
    PUBLIC Crisp::VulkanPhysicalDevice
    PUBLIC Crisp::VulkanDevice
    PUBLIC Crisp::VulkanResource
    PUBLIC Crisp::VulkanSwapChain
    PUBLIC Crisp::VulkanBuffer
    PUBLIC Crisp::VulkanImage
    PUBLIC Crisp::VulkanSampler
    PUBLIC Crisp::VulkanImageView
    PUBLIC Crisp::VulkanPipelineLayout
    PUBLIC Crisp::VulkanDescriptorSet
    PUBLIC Crisp::VulkanPipeline
    PUBLIC Crisp::VulkanFramebuffer
    PUBLIC Crisp::VulkanRenderPass
    PUBLIC Crisp::VulkanCommandBuffer
    PUBLIC Crisp::VulkanAccelerationStructure
    PUBLIC Crisp::VulkanFormatTraits
    PUBLIC Crisp::VulkanVertexLayout
)

add_cpp_interface_library(
    CrispVulkanFormatTraits
    "VulkanVertexLayout.hpp"
)
target_link_libraries(
    CrispVulkanFormatTraits
    INTERFACE Crisp::VulkanHeader
)

add_cpp_static_library(
    CrispVulkanVertexLayout
    "VulkanVertexLayout.cpp"
    "VulkanVertexLayout.hpp"
)
target_link_libraries(
    CrispVulkanVertexLayout
    PUBLIC Crisp::VulkanHeader
    PUBLIC Crisp::VulkanFormatTraits
)

add_cpp_test(
    CrispVulkanVertexLayoutTest
    "Test/VulkanVertexLayoutTest.cpp"
)
target_link_libraries(
    CrispVulkanVertexLayoutTest
    PRIVATE Crisp::VulkanVertexLayout
)

add_cpp_static_library(
    CrispVulkanRingBuffer
    "VulkanRingBuffer.cpp"
    "VulkanRingBuffer.hpp"
)
target_link_libraries(
    CrispVulkanRingBuffer
    PUBLIC Crisp::VulkanBuffer
    PUBLIC Microsoft.GSL::GSL
)

add_cpp_test(
    CrispVulkanRingBufferTest
    "Test/VulkanRingBufferTest.cpp"
)
target_link_libraries(
    CrispVulkanRingBufferTest
    PRIVATE Crisp::VulkanRingBuffer
    PRIVATE Crisp::VulkanTestUtils
)

add_cpp_interface_library(
    CrispVulkanSynchronization
    "VulkanSynchronization.hpp"
)
target_link_libraries(
    CrispVulkanSynchronization
    INTERFACE Crisp::VulkanHeader
)

add_cpp_test(
    CrispVulkanSynchronizationTest
    "Test/VulkanSynchronizationTest.cpp"
)
target_link_libraries(
    CrispVulkanSynchronizationTest
    PRIVATE Crisp::VulkanSynchronization
)

add_cpp_static_library(
    CrispVulkanCommandEncoder
    "VulkanCommandEncoder.cpp"
    "VulkanCommandEncoder.hpp"
)
target_link_libraries(
    CrispVulkanCommandEncoder
    PUBLIC Crisp::Vulkan
    PUBLIC Crisp::VulkanSynchronization
)

add_cpp_static_library(
    CrispVulkanTracer
    "VulkanTracer.cpp"
    "VulkanTracer.hpp"
)
target_link_libraries(
    CrispVulkanTracer
    PUBLIC Crisp::Vulkan
    PUBLIC Crisp::ChromeEventTracer
    PUBLIC Microsoft.GSL::GSL
)