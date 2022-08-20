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
    "RenderPasses/VarianceShadowMapPass.cpp"
    "RenderPasses/VarianceShadowMapPass.hpp"
    "BufferUpdatePolicy.hpp"
    "ComputePipeline.cpp"
    "ComputePipeline.hpp"
    "DrawCommand.hpp"
    "IndexBuffer.cpp"
    "IndexBuffer.hpp"
    "LuaPipelineBuilder.cpp"
    "LuaPipelineBuilder.hpp"
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
)
target_include_directories(CrispRenderer PUBLIC ".")
target_compile_definitions(CrispRenderer PUBLIC __TBB_NO_IMPLICIT_LINKAGE)
target_link_libraries(CrispRenderer
    PUBLIC CrispBitFlags
    PUBLIC CrispVulkan
    PUBLIC CrispImageIo
    PUBLIC CrispMath
    PUBLIC CrispUtils
    PUBLIC CrispGeometry
    PUBLIC CrispRobinHood
    PUBLIC CrispResult
    PUBLIC CrispCoreUtils
    PUBLIC CrispRendererConfig
    PUBLIC tbb
    PRIVATE CrispFileUtils
    PUBLIC CrispGlslUtils
    PRIVATE CrispApplicationEnvironment)

add_cpp_static_library(CrispImageCache
    "ImageCache.hpp"
    "ImageCache.cpp"
)
target_include_directories(CrispImageCache PUBLIC ".")
target_link_libraries(CrispImageCache
    PUBLIC CrispVulkan
    PRIVATE CrispRenderer
)

add_cpp_interface_library(CrispRendererConfig
    "RendererConfig.hpp"
)
target_include_directories(CrispRendererConfig INTERFACE ".")
