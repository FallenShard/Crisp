#include <Crisp/Renderer/RenderPasses/CubeMapRenderPass.hpp>

#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {

class CubeMapPassTest : public VulkanTest {};

TEST_F(CubeMapPassTest, DeclaresInternalCubeCompatibleOutput) {
    rg::RenderGraph renderGraph;
    const auto cubeMap = addCubeMapPass(
        renderGraph, "cube-map-pass", {1024, 1024}, VK_FORMAT_R16G16B16A16_SFLOAT, [](const FrameContext&) {});

    const rg::RenderGraph& graph = renderGraph;
    ASSERT_EQ(graph.getPassCount(), 1);
    ASSERT_EQ(graph.getResourceCount(), 1);
    EXPECT_EQ(graph.getImageDescription(cubeMap).layerCount, 6);
    EXPECT_TRUE(graph.getImageDescription(cubeMap).createFlags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
    EXPECT_FALSE(graph.getResources()[cubeMap.id].externalAccess.has_value());
}

TEST_F(CubeMapPassTest, Executes) {
    rg::RenderGraph renderGraph;
    addCubeMapPass(renderGraph, "cube-map-pass", {256, 256}, VK_FORMAT_R16G16B16A16_SFLOAT, [](const FrameContext&) {});

    renderGraph.compile(*device_, {256, 256});
    ScopeCommandExecutor executor(*device_);
    renderGraph.execute(FrameContext{.commandEncoder = VulkanCommandEncoder{executor.cmdBuffer.getHandle()}});
}

} // namespace
} // namespace crisp
