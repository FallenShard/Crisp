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
    "VulkanMemoryAllocator.cpp"
    "VulkanMemoryAllocator.hpp"
    "VulkanMemoryHeap.cpp"
    "VulkanMemoryHeap.hpp"
    "VulkanResourceDeallocator.cpp"
    "VulkanResourceDeallocator.hpp"
    "VulkanQueue.cpp"
    "VulkanQueue.hpp"
    "VulkanQueueConfiguration.cpp"
    "VulkanQueueConfiguration.hpp"
)
target_link_libraries(
    CrispVulkanDevice
    PUBLIC Crisp::VulkanInstance
    PUBLIC Crisp::VulkanPhysicalDevice
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
    CrispVulkanMemoryAllocatorTest
    "Test/VulkanMemoryAllocatorTest.cpp"
)
target_link_libraries(
    CrispVulkanMemoryAllocatorTest
    PRIVATE Crisp::VulkanDevice
    PRIVATE Crisp::VulkanTestUtils
)

add_cpp_test(
    CrispVulkanMemoryHeapTest
    "Test/VulkanMemoryHeapTest.cpp"
)
target_link_libraries(
    CrispVulkanMemoryHeapTest
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