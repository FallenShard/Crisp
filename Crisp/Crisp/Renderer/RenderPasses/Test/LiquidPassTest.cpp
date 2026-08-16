#include <Crisp/Renderer/RenderPasses/LiquidRenderPass.hpp>

#include <gtest/gtest.h>

namespace crisp {
namespace {

TEST(LiquidPassTest, DeclaresGeometryAndCompositeResources) {
    rg::RenderGraph renderGraph;
    const auto liquid = addLiquidPasses(
        renderGraph,
        "liquid-pass",
        {1280, 720},
        VK_FORMAT_R16G16B16A16_SFLOAT,
        [](const FrameContext&) {},
        [](const FrameContext&) {});

    const rg::RenderGraph& graph = renderGraph;
    ASSERT_EQ(graph.getPassCount(), 2);
    ASSERT_EQ(graph.getResourceCount(), 3);
    EXPECT_FALSE(graph.getResources()[liquid.image.id].externalAccess.has_value());

    const auto& liquidCompositePass = graph.getPass(RenderGraphPassHandle{1});
    ASSERT_EQ(liquidCompositePass.inputs.size(), 1);
    EXPECT_EQ(liquidCompositePass.inputs[0].id, liquid.sceneColor.id);
}

} // namespace
} // namespace crisp
