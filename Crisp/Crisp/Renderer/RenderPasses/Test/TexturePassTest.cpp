#include <Crisp/Renderer/RenderPasses/TexturePass.hpp>

#include <gtest/gtest.h>

namespace crisp {
namespace {

TEST(TexturePassTest, DeclaresInternalTransferSourceOutput) {
    rg::RenderGraph renderGraph;
    const auto texture = addTexturePass(
        renderGraph, "texture-pass", {1920, 1080}, VK_FORMAT_R8G8B8A8_UNORM, true, [](const FrameContext&) {});

    const rg::RenderGraph& graph = renderGraph;
    ASSERT_EQ(graph.getPassCount(), 1);
    ASSERT_EQ(graph.getResourceCount(), 1);
    EXPECT_EQ(graph.getImageDescription(texture).sizePolicy, SizePolicy::SwapChainRelative);
    EXPECT_TRUE(graph.getImageDescription(texture).imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    EXPECT_FALSE(graph.getResources()[texture.id].externalAccess.has_value());
}

} // namespace
} // namespace crisp
