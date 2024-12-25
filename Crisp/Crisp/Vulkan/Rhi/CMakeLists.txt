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
    PUBLIC Crisp::HashMap
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
)
target_link_libraries(
    CrispVulkanDevice
    PUBLIC Crisp::VulkanInstance
    PUBLIC Crisp::VulkanPhysicalDevice
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