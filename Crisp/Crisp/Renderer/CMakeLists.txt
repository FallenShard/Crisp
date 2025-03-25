add_subdirectory(RenderGraph)

add_cpp_static_library(
    CrispShaderCache
    "ShaderCache.hpp"
    "ShaderCache.cpp"
)
target_link_libraries(
    CrispShaderCache
    PUBLIC Crisp::Vulkan
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::Logger
    PRIVATE Crisp::FileUtils
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
    PUBLIC nlohmann_json::nlohmann_json
    PUBLIC Crisp::ThreadPool
    PUBLIC Crisp::Renderer
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

add_cpp_interface_library(
    CrispAssetPaths
    "AssetPaths.hpp"
)

# add_cpp_static_library(
# CrispRenderPasses
# "RenderPasses/BlurPass.cpp"
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
    CrispCubeMapRenderPass
    "RenderPasses/CubeMapRenderPass.cpp"
    "RenderPasses/CubeMapRenderPass.hpp"
)
target_link_libraries(
    CrispCubeMapRenderPass
    PUBLIC Crisp::Renderer
)

add_cpp_static_library(
    CrispTexturePass
    "RenderPasses/TexturePass.cpp"
    "RenderPasses/TexturePass.hpp"
)
target_link_libraries(
    CrispTexturePass
    PUBLIC Crisp::Renderer
)

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
    "RenderGraph.cpp"
    "RenderGraph.hpp"
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
    "RenderTargetCache.cpp"
    "RenderTargetCache.hpp"
)
target_link_libraries(CrispRenderer
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
    PUBLIC Crisp::ShaderCache
    PUBLIC Crisp::VulkanRingBuffer
    PUBLIC Crisp::VulkanCommandEncoder
    PUBLIC Crisp::AssetPaths
    PUBLIC Crisp::VulkanTracer
    PUBLIC Crisp::VulkanRenderPassBuilder
)

add_cpp_static_library(CrispImageCache
    "ImageCache.hpp"
    "ImageCache.cpp"
)
target_link_libraries(CrispImageCache
    PUBLIC Crisp::Vulkan
    PRIVATE Crisp::Renderer
)
add_cpp_interface_library(CrispFrameContext
    "FrameContext.hpp"
)
target_link_libraries(CrispFrameContext
    INTERFACE Crisp::Vulkan
)
add_cpp_interface_library(CrispRendererConfig
    "RendererConfig.hpp"
)

add_cpp_test(CrispDescriptorUpdaterTest
    "Test/DescriptorUpdaterTest.cpp")
target_link_libraries(CrispDescriptorUpdaterTest PRIVATE Crisp::Renderer CrispVulkanTestUtils)

add_cpp_static_library(CrispRayTracingPipelineBuilder
    "RayTracingPipelineBuilder.hpp"
    "RayTracingPipelineBuilder.cpp"
)
target_link_libraries(CrispRayTracingPipelineBuilder
    PUBLIC Crisp::Renderer
)