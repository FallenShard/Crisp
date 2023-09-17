add_cpp_static_library(CrispRenderer
    "RenderPasses/BlurPass.cpp"
    "RenderPasses/BlurPass.hpp"
    "RenderPasses/CubeMapRenderPass.cpp"
    "RenderPasses/CubeMapRenderPass.hpp"
    "RenderPasses/DepthPass.cpp"
    "RenderPasses/DepthPass.hpp"
    "RenderPasses/ForwardLightingPass.cpp"
    "RenderPasses/ForwardLightingPass.hpp"
    "RenderPasses/LightShaftPass.cpp"
    "RenderPasses/LightShaftPass.hpp"
    "RenderPasses/LiquidRenderPass.cpp"
    "RenderPasses/LiquidRenderPass.hpp"
    "RenderPasses/ShadowPass.cpp"
    "RenderPasses/ShadowPass.hpp"
    "RenderPasses/TexturePass.cpp"
    "RenderPasses/TexturePass.hpp"
    "BufferUpdatePolicy.hpp"
    "ComputePipeline.cpp"
    "ComputePipeline.hpp"
    "DrawCommand.hpp"
    "IndexBuffer.cpp"
    "IndexBuffer.hpp"
    "Material.cpp"
    "Material.hpp"
    "PipelineBuilder.cpp"
    "PipelineBuilder.hpp"
    "PipelineLayoutBuilder.cpp"
    "PipelineLayoutBuilder.hpp"
    "Renderer.cpp"
    "Renderer.hpp"
    "RendererConfig.hpp"
    "RenderGraph.cpp"
    "RenderGraph.hpp"
    "RenderNode.cpp"
    "RenderNode.hpp"
    "RenderPassBuilder.cpp"
    "RenderPassBuilder.hpp"
    "ResourceContext.cpp"
    "ResourceContext.hpp"
    "Texture.cpp"
    "Texture.hpp"
    "UniformBuffer.cpp"
    "UniformBuffer.hpp"
    "UniformMultiBuffer.cpp"
    "UniformMultiBuffer.hpp"
    "VertexBuffer.cpp"
    "VertexBuffer.hpp"
    "RendererFrame.cpp"
    "RendererFrame.hpp"
    "VulkanBufferUtils.cpp"
    "VulkanBufferUtils.hpp"
    "VulkanImageUtils.cpp"
    "VulkanImageUtils.hpp"
    "VulkanWorker.cpp"
    "VulkanWorker.hpp"
    "StorageBuffer.hpp"
    "StorageBuffer.cpp"
    "ImageCache.hpp"
    "ImageCache.cpp"
    "PipelineCache.cpp"
    "PipelineCache.hpp"
    "RenderTargetCache.cpp"
    "RenderTargetCache.hpp"
    "IO/JsonPipelineBuilder.cpp"
    "IO/JsonPipelineBuilder.hpp"
    "IO/LuaPipelineBuilder.cpp"
    "IO/LuaPipelineBuilder.hpp"
    "AssetPaths.hpp"
    "ShaderCache.hpp"
    "ShaderCache.cpp"
)
target_link_libraries(CrispRenderer
    PUBLIC Crisp::BitFlags
    PUBLIC Crisp::Vulkan
    PUBLIC Crisp::ImageIo
    PUBLIC Crisp::Math
    PUBLIC Crisp::Utils
    PUBLIC Crisp::Geometry
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::Result
    PUBLIC Crisp::CoreUtils
    PUBLIC Crisp::RendererConfig
    PUBLIC Crisp::FrameContext
    PUBLIC tbb
    PRIVATE Crisp::FileUtils
    PUBLIC Crisp::SpvReflection
    PRIVATE Crisp::ApplicationEnvironment
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE Crisp::ShaderCompiler
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

add_cpp_test(CrispPipelineLayoutBuilderTest
    "Test/PipelineLayoutBuilderTest.cpp")
target_link_libraries(CrispPipelineLayoutBuilderTest PRIVATE Crisp::Renderer CrispVulkanTestUtils)
copy_shared_libs(CrispPipelineLayoutBuilderTest lua)

add_cpp_static_library(CrispVulkanRenderPassBuilder
    "VulkanRenderPassBuilder.hpp"
    "VulkanRenderPassBuilder.cpp"
)
target_link_libraries(CrispVulkanRenderPassBuilder
    PUBLIC Crisp::Vulkan
)

add_cpp_static_library(CrispRenderGraph
    "RenderGraphBlackboard.hpp"
    "RenderGraphExperimental.cpp"
    "RenderGraphExperimental.hpp"
    "RenderGraphExperimentalUtils.hpp"
)
target_link_libraries(CrispRenderGraph
    PUBLIC Crisp::Vulkan
    PRIVATE Crisp::VulkanRenderPassBuilder
    PRIVATE Crisp::Renderer
)

add_cpp_test(CrispRenderGraphTest
    "Test/RenderGraphTest.cpp"
)
target_link_libraries(CrispRenderGraphTest PRIVATE Crisp::VulkanTestUtils Crisp::RenderGraph)
