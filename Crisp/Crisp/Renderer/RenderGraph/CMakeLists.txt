add_cpp_static_library(CrispRenderGraph
    "RenderGraphBlackboard.hpp"
    "RenderGraph.cpp"
    "RenderGraph.hpp"
    "RenderGraphUtils.hpp"
    "RenderGraphHandles.hpp"
    "RenderGraphIo.cpp"
    "RenderGraphIo.hpp"
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