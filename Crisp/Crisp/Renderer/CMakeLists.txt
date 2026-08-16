add_subdirectory(RenderGraph)
add_subdirectory(RenderPasses)

add_cpp_header_library(
    CrispAssetPaths
    "AssetPaths.hpp"
)

add_cpp_static_library(
    CrispBindlessImageRegistry
    "BindlessImageRegistry.cpp"
    "BindlessImageRegistry.hpp"
)
target_link_libraries(
    CrispBindlessImageRegistry
    PUBLIC Crisp::Vulkan
)

add_cpp_test(
    CrispBindlessImageRegistryTest
    "Test/BindlessImageRegistryTest.cpp"
)
target_link_libraries(
    CrispBindlessImageRegistryTest
    PRIVATE Crisp::BindlessImageRegistry
    PRIVATE Crisp::VulkanTestUtils
)

# add_cpp_static_library(
# CrispRenderPasses
# "RenderPasses/BlurPass.hpp"
# "RenderPasses/CubeMapRenderPass.cpp"
# "RenderPasses/CubeMapRenderPass.hpp"
# "RenderPasses/DepthPass.cpp"
# "RenderPasses/DepthPass.hpp"
# "RenderPasses/ForwardLightingPass.cpp"
# "RenderPasses/ForwardLightingPass.hpp"
# "RenderPasses/LightShaftPass.cpp"
# "RenderPasses/LightShaftPass.hpp"
# "RenderPasses/LiquidRenderPass.cpp"
# "RenderPasses/LiquidRenderPass.hpp"
# "RenderPasses/ShadowPass.cpp"
# "RenderPasses/ShadowPass.hpp"
# "RenderPasses/TexturePass.cpp"
# "RenderPasses/TexturePass.hpp"
# )
add_cpp_static_library(
    CrispRenderer
    "BufferUpdatePolicy.hpp"
    "ComputePipeline.cpp"
    "ComputePipeline.hpp"
    "DrawCommand.hpp"
    "Material.cpp"
    "Material.hpp"
    "Renderer.cpp"
    "Renderer.hpp"
    "RendererConfig.hpp"
    "RenderNode.cpp"
    "RenderNode.hpp"
    "ResourceContext.cpp"
    "ResourceContext.hpp"
    "RendererFrame.cpp"
    "RendererFrame.hpp"
    "VulkanBufferUtils.cpp"
    "VulkanBufferUtils.hpp"
    "VulkanImageUtils.cpp"
    "VulkanImageUtils.hpp"
    "VulkanWorker.cpp"
    "VulkanWorker.hpp"
    "ImageCache.hpp"
    "ImageCache.cpp"
    "PipelineCache.cpp"
    "PipelineCache.hpp"
)
target_link_libraries(CrispRenderer
    PUBLIC Crisp::BindlessImageRegistry
    PUBLIC Crisp::BitFlags
    PUBLIC Crisp::Vulkan
    PUBLIC Crisp::ImageIo
    PUBLIC Crisp::Math
    PUBLIC Crisp::Geometry
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::Result
    PUBLIC Crisp::ThreadPool
    PUBLIC Crisp::RendererConfig
    PUBLIC Crisp::FrameContext
    PUBLIC tbb
    PUBLIC Crisp::SpvReflection
    PRIVATE Crisp::ApplicationEnvironment
    PRIVATE Crisp::ShaderCompiler
    PUBLIC Crisp::PipelineBuilder
    PUBLIC Crisp::VulkanRingBuffer
    PUBLIC Crisp::VulkanCommandEncoder
    PUBLIC Crisp::AssetPaths
    PUBLIC Crisp::VulkanTracer
    PUBLIC Crisp::VulkanStagingBelt
)

add_cpp_static_library(CrispImageCache
    "ImageCache.hpp"
    "ImageCache.cpp"
)
target_link_libraries(CrispImageCache
    PUBLIC Crisp::Vulkan
    PRIVATE Crisp::Renderer
)
add_cpp_header_library(CrispFrameContext
    "FrameContext.hpp"
)
target_link_libraries(CrispFrameContext
    INTERFACE Crisp::Vulkan
)
add_cpp_header_library(CrispRendererConfig
    "RendererConfig.hpp"
)

add_cpp_test(CrispDescriptorUpdaterTest
    "Test/DescriptorUpdaterTest.cpp")
target_link_libraries(CrispDescriptorUpdaterTest PRIVATE Crisp::Renderer CrispVulkanTestUtils)
target_add_test_shader(CrispDescriptorUpdaterTest "Test/Data/bindless.frag")
