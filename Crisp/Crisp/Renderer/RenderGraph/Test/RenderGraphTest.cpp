#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

#include <Crisp/Renderer/RenderGraph/RenderGraphIo.hpp>
#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSwapChain.hpp>

namespace crisp {
namespace {

using ::testing::SizeIs;

struct FluidSimulationData {
    RenderGraphResourceHandle positionBuffer;
};

struct DepthPrePassData {
    RenderGraphResourceHandle depthImage;
};

struct ForwardLightingData {
    RenderGraphResourceHandle hdrImage;
};

class RenderGraphTest : public VulkanTestWithSurface {
protected:
    static VulkanSwapChain createSwapChain(const PresentationMode presentationMode = PresentationMode::TripleBuffered) {
        return {*device_, *physicalDevice_, instance_->getSurface(), presentationMode};
    }
};

VkClearValue createDepthClearValue(const float depthValue, const uint8_t stencilValue) {
    VkClearValue v{};
    v.depthStencil = {depthValue, stencilValue};
    return v;
}

TEST(RenderGraphTest2, BasicUsage) {
    rg::RenderGraph rg;
    rg.addPass(
        "fluid-pass",
        [](rg::RenderGraph::Builder& builder) {
            auto& data = builder.getBlackboard().insert<FluidSimulationData>();
            data.positionBuffer = builder.createBuffer({}, "fluid-position-buffer");
        },
        [](const FrameContext&) {});

    rg.addPass(
        "depth-pre-pass",
        [](rg::RenderGraph::Builder& builder) {
            builder.readBuffer(builder.getBlackboard().get<FluidSimulationData>().positionBuffer);

            auto& data = builder.getBlackboard().insert<DepthPrePassData>();
            data.depthImage = builder.createAttachment(
                {.sizePolicy = SizePolicy::SwapChainRelative, .format = VK_FORMAT_D24_UNORM_S8_UINT},
                "depth-buffer",
                createDepthClearValue(1.0f, 0));
        },
        [](const FrameContext&) {});

    RenderGraphResourceHandle output{};
    rg.addPass(
        "spectrum-pass",
        [&output](rg::RenderGraph::Builder& builder) {
            output = builder.createStorageImage({.format = VK_FORMAT_R32G32_SFLOAT}, "spectrum-image");
        },
        [](const FrameContext&) {});

    for (uint32_t i = 0; i < 10; ++i) {
        rg.addPass(
            fmt::format("fft-pass-{}", i),
            [&output, i](rg::RenderGraph::Builder& builder) {
                builder.readStorageImage(output);
                output = builder.createStorageImage(
                    {.format = VK_FORMAT_R32G32_SFLOAT}, fmt::format("fft-pass-image-{}", i));
            },
            [](const FrameContext&) {});
    }

    rg.addPass(
        "forward-pass",
        [&output](rg::RenderGraph::Builder& builder) {
            builder.readAttachment(builder.getBlackboard().get<DepthPrePassData>().depthImage);
            builder.readBuffer(builder.getBlackboard().get<FluidSimulationData>().positionBuffer);
            builder.readStorageImage(output);
            auto& data = builder.getBlackboard().insert<ForwardLightingData>();
            data.hdrImage = builder.createAttachment({.format = VK_FORMAT_R32G32B32A32_SFLOAT}, "hdr-image");
        },
        [](const FrameContext&) {});

    RenderGraphResourceHandle modifiedHdrImage{};
    rg.addPass(
        "transparent-pass",
        [&modifiedHdrImage](rg::RenderGraph::Builder& builder) {
            builder.readAttachment(builder.getBlackboard().get<DepthPrePassData>().depthImage);
            modifiedHdrImage = builder.writeAttachment(builder.getBlackboard().get<ForwardLightingData>().hdrImage);
        },
        [](const FrameContext&) {});

    RenderGraphResourceHandle bloomImage{};
    rg.addPass(
        "bloom-pass",
        [modifiedHdrImage, &bloomImage](rg::RenderGraph::Builder& builder) {
            builder.readTexture(modifiedHdrImage);
            bloomImage = builder.createAttachment({.format = VK_FORMAT_R32G32B32A32_SFLOAT}, "bloom-image");
        },
        [](const FrameContext&) {});

    rg.addPass(
        "tonemapping-pass",
        [bloomImage](rg::RenderGraph::Builder& builder) {
            builder.readTexture(bloomImage);
            builder.createAttachment({.format = VK_FORMAT_R8G8B8A8_UNORM}, "ldr-image");
        },
        [](const FrameContext&) {});

    toGraphViz(rg, "D:/graph.dot").unwrap();

    EXPECT_THAT(rg.getPassCount(), 17);
    EXPECT_THAT(rg.getResourceCount(), 17);
}

TEST_F(RenderGraphTest, BasicUsage) {
    rg::RenderGraph rg;

    rg.addPass(
        "forward-pass",
        [](rg::RenderGraph::Builder& builder) {
            builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_D24_UNORM_S8_UINT,
                },
                "depth-buffer",
                createDepthClearValue(1.0f, 0));
            auto& data = builder.getBlackboard().insert<ForwardLightingData>();
            data.hdrImage = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                "hdr-image");
        },
        [](const FrameContext&) {});

    RenderGraphResourceHandle bloomImage{};
    rg.addPass(
        "bloom-pass",
        [&bloomImage](rg::RenderGraph::Builder& builder) {
            const auto& forwardData = builder.getBlackboard().get<ForwardLightingData>();
            builder.readTexture(forwardData.hdrImage);
            bloomImage = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                "bloom-image");
        },
        [](const FrameContext&) {});

    constexpr VkExtent2D kSwapChainExtent{1600, 900};
    rg.compile(*device_, kSwapChainExtent);

    {
        ScopeCommandExecutor executor(*device_);
        FrameContext context{.commandEncoder = VulkanCommandEncoder{executor.cmdBuffer.getHandle()}};
        rg.execute(context);
    }

    toGraphViz(rg, "D:/graph_small.dot").unwrap();

    EXPECT_THAT(rg.getPassCount(), 2);
    EXPECT_THAT(rg.getResourceCount(), 3);
}

TEST(RenderGraphTest2, Blackboard) {
    RenderGraphBlackboard bb{};

    struct MyStuff {
        RenderGraphResourceHandle img0{};
        RenderGraphResourceHandle img1{};
    };

    struct MyStuff1 {};

    auto& s1 = bb.insert<MyStuff>();
    s1.img0 = {123};
    s1.img1 = {111};

    bb.insert<MyStuff1>();
    EXPECT_THAT(bb, SizeIs(2));

    const auto& s2 = bb.get<MyStuff>();
    EXPECT_EQ(s2.img0.id, 123);
    EXPECT_EQ(s2.img1.id, 111);

    // Attempting to add another struct results in termination.
    EXPECT_DEATH_IF_SUPPORTED(bb.insert<MyStuff1>(), "");
}

} // namespace
} // namespace crisp