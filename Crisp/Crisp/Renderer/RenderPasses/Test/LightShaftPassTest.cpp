#include <Crisp/Renderer/RenderPasses/LightShaftPass.hpp>

#include <gtest/gtest.h>

namespace crisp {
namespace {

TEST(LightShaftPassTest, DeclaresRequestedInternalOutput) {
    rg::RenderGraph renderGraph;
    const auto lightShaft = addLightShaftPass(
        renderGraph,
        "light-shaft-pass",
        {
            .sizePolicy = SizePolicy::Absolute,
            .width = 640,
            .height = 360,
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        },
        [](const FrameContext&) {});

    const rg::RenderGraph& graph = renderGraph;
    ASSERT_EQ(graph.getPassCount(), 1);
    ASSERT_EQ(graph.getResourceCount(), 1);
    EXPECT_EQ(graph.getImageDescription(lightShaft).width, 640);
    EXPECT_EQ(graph.getImageDescription(lightShaft).height, 360);
    EXPECT_FALSE(graph.getResources()[lightShaft.id].externalAccess.has_value());
}

} // namespace
} // namespace crisp
