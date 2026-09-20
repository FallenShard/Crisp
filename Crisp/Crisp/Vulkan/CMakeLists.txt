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
    PUBLIC Crisp::VulkanDescriptorHeap
    PUBLIC Crisp::VulkanPipeline
    PUBLIC Crisp::VulkanRasterizationPassDescriptor
    PUBLIC Crisp::VulkanCommandBuffer
    PUBLIC Crisp::VulkanAccelerationStructure
    PUBLIC Crisp::VulkanFormatTraits
    PUBLIC Crisp::VulkanVertexLayout
)

add_cpp_header_library(
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
    CrispVulkanStagingBuffer
    "VulkanStagingBuffer.cpp"
    "VulkanStagingBuffer.hpp"
)
target_link_libraries(
    CrispVulkanStagingBuffer
    PUBLIC Crisp::VulkanBuffer
    PRIVATE Crisp::VulkanCommandEncoder
)

add_cpp_test(
    CrispVulkanStagingBufferTest
    "Test/VulkanStagingBufferTest.cpp"
)
target_link_libraries(
    CrispVulkanStagingBufferTest
    PRIVATE Crisp::VulkanStagingBuffer
    PRIVATE Crisp::VulkanTestUtils
)

add_cpp_static_library(
    CrispVulkanDescriptorHeap
    "VulkanDescriptorHeap.cpp"
    "VulkanDescriptorHeap.hpp"
)
target_link_libraries(
    CrispVulkanDescriptorHeap
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::SpvReflection
    PUBLIC Crisp::VulkanAccelerationStructure
    PUBLIC Crisp::VulkanBuffer
    PUBLIC Crisp::VulkanImageView
)

add_cpp_static_library(
    CrispVulkanStagingBelt
    "VulkanStagingBelt.cpp"
    "VulkanStagingBelt.hpp"
)
target_link_libraries(
    CrispVulkanStagingBelt
    PUBLIC Crisp::VulkanStagingBuffer
    PUBLIC Crisp::VulkanCommandEncoder
    PUBLIC Crisp::VulkanDescriptorHeap
    PRIVATE Crisp::VulkanImage
)

add_cpp_test(
    CrispVulkanStagingBeltTest
    "Test/VulkanStagingBeltTest.cpp"
)
target_link_libraries(
    CrispVulkanStagingBeltTest
    PRIVATE Crisp::VulkanStagingBelt
    PRIVATE Crisp::VulkanTestUtils
)

add_cpp_static_library(
    CrispVulkanRingBuffer
    "VulkanRingBuffer.cpp"
    "VulkanRingBuffer.hpp"
)
target_link_libraries(
    CrispVulkanRingBuffer
    PUBLIC Crisp::VulkanBuffer
    PUBLIC Crisp::VulkanCommandEncoder
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

add_cpp_header_library(
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
    PUBLIC Crisp::VulkanCommandEncoder
    PUBLIC Crisp::ChromeEventTracer
    PUBLIC Microsoft.GSL::GSL
)

add_cpp_static_library(
    CrispPipelineBuilder
    "PipelineBuilder.cpp"
    "PipelineBuilder.hpp"
    "PipelineLayoutBuilder.cpp"
    "PipelineLayoutBuilder.hpp"
    "VulkanPipelineIo.cpp"
    "VulkanPipelineIo.hpp"
)
target_link_libraries(
    CrispPipelineBuilder
    PUBLIC Crisp::Vulkan
    PUBLIC Crisp::Result
    PUBLIC Crisp::SpvReflection
    PRIVATE Crisp::Checks
    PRIVATE Crisp::HashMap
    PRIVATE Crisp::JsonUtils
    PRIVATE Crisp::Logger
)

add_cpp_test(
    CrispPipelineBuilderTest
    "Test/PipelineBuilderTest.cpp"
)
target_link_libraries(
    CrispPipelineBuilderTest
    PRIVATE Crisp::PipelineBuilder
)

add_cpp_test(
    CrispPipelineLayoutBuilderTest
    "Test/PipelineLayoutBuilderTest.cpp"
)
target_link_libraries(
    CrispPipelineLayoutBuilderTest
    PRIVATE Crisp::PipelineBuilder
    PRIVATE Crisp::VulkanTestUtils
)

add_cpp_static_library(
    CrispRayTracingPipelineBuilder
    "RayTracingPipelineBuilder.hpp"
    "RayTracingPipelineBuilder.cpp"
)
target_link_libraries(
    CrispRayTracingPipelineBuilder
    PUBLIC Crisp::Vulkan
    PRIVATE Crisp::ShaderType
    PRIVATE Crisp::SpvReflection
    PRIVATE Crisp::VulkanCommandEncoder
)

add_cpp_static_library(
    CrispVulkanBenchmark
    "Benchmark/VulkanBenchmark.cpp"
    "Benchmark/VulkanBenchmark.hpp"
)
target_link_libraries(
    CrispVulkanBenchmark
    PUBLIC Crisp::Vulkan
    PUBLIC Crisp::VulkanCommandEncoder
    PUBLIC benchmark::benchmark
    PRIVATE Crisp::ShaderCompiler
)
target_compile_definitions(
    CrispVulkanBenchmark
    PUBLIC CRISP_BENCHMARK_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Benchmark/Shaders"
)

add_cpp_benchmark(
    CrispBandwidthBenchmark
    "Benchmark/BandwidthBenchmark.cpp"
    DEPS
    Crisp::VulkanBenchmark
    Crisp::Renderer
)

add_cpp_benchmark(
    CrispFlopsBenchmark
    "Benchmark/FlopsBenchmark.cpp"
    DEPS
    Crisp::VulkanBenchmark
    Crisp::Renderer
)

add_cpp_benchmark(
    CrispTrisBenchmark
    "Benchmark/TrisBenchmark.cpp"
    DEPS
    Crisp::VulkanBenchmark
    Crisp::PipelineBuilder
    Crisp::SpvReflection
)
