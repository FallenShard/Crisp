#include <Crisp/Renderer/RenderPasses/BlurPass.hpp>

#include <gtest/gtest.h>

namespace crisp {
namespace {

TEST(BlurPassTest, DeclaresInternalOutput) {
    rg::RenderGraph renderGraph;
    RenderGraphResourceHandle source;
    renderGraph.addPass(
        "source-pass",
        [&source](rg::RenderGraph::Builder& builder) {
            source = builder.createAttachment({.format = VK_FORMAT_R16G16B16A16_SFLOAT}, "source-image");
        },
        [](const FrameContext&) {});

    const auto output = addBlurPass(
        renderGraph, "blur-pass", source, VK_FORMAT_R16G16B16A16_SFLOAT, {1920, 1080}, true, [](const FrameContext&) {});

    const rg::RenderGraph& graph = renderGraph;
    ASSERT_EQ(graph.getPassCount(), 2);
    ASSERT_EQ(graph.getResourceCount(), 2);
    const auto& blurPass = graph.getPass(RenderGraphPassHandle{1});
    ASSERT_EQ(blurPass.inputs.size(), 1);
    EXPECT_EQ(blurPass.inputs[0].resource.id, source.id);
    EXPECT_EQ(graph.getImageDescription(output).sizePolicy, SizePolicy::SwapChainRelative);
    EXPECT_EQ(graph.getImageDescription(output).format, VK_FORMAT_R16G16B16A16_SFLOAT);
    EXPECT_TRUE(graph.getImageDescription(output).clearValue.has_value());
    EXPECT_FALSE(graph.getResources()[output.id].externalAccess.has_value());
}

TEST(BlurPassTest, AllowsCallerToExportOutput) {
    rg::RenderGraph renderGraph;
    RenderGraphResourceHandle source;
    renderGraph.addPass(
        "source-pass",
        [&source](rg::RenderGraph::Builder& builder) {
            source = builder.createAttachment({.format = VK_FORMAT_R16G16B16A16_SFLOAT}, "source-image");
        },
        [](const FrameContext&) {});

    const auto output = addBlurPass(
        renderGraph, "blur-pass", source, VK_FORMAT_R16G16B16A16_SFLOAT, {1920, 1080}, true, [](const FrameContext&) {});

    renderGraph.exportTexture(output);
    EXPECT_TRUE(renderGraph.getResources()[output.id].externalAccess.has_value());
}

} // namespace
} // namespace crisp
