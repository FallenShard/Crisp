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
    "VirtualFrame.hpp"
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
    PUBLIC CrispBitFlags
    PUBLIC CrispVulkan
    PUBLIC CrispImageIo
    PUBLIC CrispMath
    PUBLIC CrispUtils
    PUBLIC CrispGeometry
    PUBLIC CrispHashMap
    PUBLIC CrispResult
    PUBLIC CrispCoreUtils
    PUBLIC CrispRendererConfig
    PUBLIC tbb
    PRIVATE CrispFileUtils
    PUBLIC CrispGlslUtils
    PRIVATE CrispApplicationEnvironment
    PUBLIC nlohmann_json::nlohmann_json)

add_cpp_static_library(CrispImageCache
    "ImageCache.hpp"
    "ImageCache.cpp"
)
target_link_libraries(CrispImageCache
    PUBLIC CrispVulkan
    PRIVATE CrispRenderer
)

add_cpp_interface_library(CrispRendererConfig
    "RendererConfig.hpp"
)

add_cpp_test(CrispRendererTest
    "Test/PipelineLayoutBuilderTest.cpp")
target_link_libraries(CrispRendererTest PRIVATE CrispRenderer CrispVulkanTestUtils)
copy_shared_libs(CrispRendererTest vulkan lua freetype zlib)
