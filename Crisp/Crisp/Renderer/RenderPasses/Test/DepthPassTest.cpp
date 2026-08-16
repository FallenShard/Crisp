#include <Crisp/Renderer/RenderPasses/DepthPass.hpp>

#include <gtest/gtest.h>

namespace crisp {
namespace {

TEST(DepthPassTest, DeclaresInternalSwapChainDepth) {
    rg::RenderGraph renderGraph;
    const auto depth = addDepthPass(renderGraph, "depth-pass", [](const FrameContext&) {});

    const rg::RenderGraph& graph = renderGraph;
    ASSERT_EQ(graph.getPassCount(), 1);
    ASSERT_EQ(graph.getResourceCount(), 1);
    EXPECT_EQ(graph.getImageDescription(depth).format, VK_FORMAT_D32_SFLOAT);
    EXPECT_EQ(graph.getImageDescription(depth).sizePolicy, SizePolicy::SwapChainRelative);
    EXPECT_FALSE(graph.getResources()[depth.id].externalAccess.has_value());
}

} // namespace
} // namespace crisp
