#include <Crisp/Vulkan/Test/VulkanTest.hpp>

#include <Crisp/Vulkan/VulkanSwapChain.hpp>

namespace crisp::test
{
namespace
{
using ::testing::Each;

class VulkanSwapChainTest : public VulkanTestWithSurface
{
protected:
    static VulkanSwapChain createSwapChain(const TripleBuffering buffering = TripleBuffering::Enabled)
    {
        return {*device_, *physicalDevice_, context_->getSurface(), buffering};
    }
};

TEST_F(VulkanSwapChainTest, Constructor)
{
    VulkanSwapChain swapChain(createSwapChain());
    EXPECT_THAT(swapChain, HandleIsValid());

    // Self-move assign
    const auto anotherSwapChain = std::move(swapChain);
    EXPECT_THAT(anotherSwapChain, HandleIsValid());
}

TEST_F(VulkanSwapChainTest, MultipleSwapChainsCanExist)
{
    std::vector<VulkanSwapChain> swapChains;
    swapChains.reserve(5);
    for (uint32_t i = 0; i < swapChains.size(); ++i)
    {
        swapChains.push_back(createSwapChain());
    }

    EXPECT_THAT(swapChains, Each(HandleIsValid()));
}

TEST_F(VulkanSwapChainTest, Bounds)
{
    const VulkanSwapChain swapChain(createSwapChain());
    EXPECT_THAT(swapChain, HandleIsValid());

    EXPECT_EQ(swapChain.getImageFormat(), VK_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(swapChain.getExtent().width, kDefaultWidth);
    EXPECT_EQ(swapChain.getExtent().height, kDefaultHeight);

    const auto extent = swapChain.getExtent();
    const auto viewport = swapChain.getViewport();
    EXPECT_EQ(extent.width, viewport.width);
    EXPECT_EQ(extent.height, viewport.height);
    EXPECT_EQ(viewport.x, 0);
    EXPECT_EQ(viewport.y, 0);
    EXPECT_EQ(viewport.minDepth, 0);
    EXPECT_EQ(viewport.maxDepth, 1);

    const auto scissor = swapChain.getScissorRect();
    EXPECT_EQ(extent.width, scissor.extent.width);
    EXPECT_EQ(extent.height, scissor.extent.height);
    EXPECT_EQ(scissor.offset.x, 0);
    EXPECT_EQ(scissor.offset.y, 0);
}

TEST_F(VulkanSwapChainTest, SwapImagesTripleBuffering)
{
    const VulkanSwapChain swapChain(createSwapChain());
    EXPECT_THAT(swapChain, HandleIsValid());
    EXPECT_EQ(swapChain.getImageCount(), 3u);
}

TEST_F(VulkanSwapChainTest, SwapImagesAreDifferent)
{
    const VulkanSwapChain swapChain(createSwapChain(TripleBuffering::Disabled));
    EXPECT_THAT(swapChain, HandleIsValid());

    EXPECT_EQ(swapChain.getImageCount(), 2u);
    EXPECT_NE(swapChain.getImageView(0), swapChain.getImageView(1));
}

TEST_F(VulkanSwapChainTest, Recreate)
{
    VulkanSwapChain swapChain(createSwapChain(TripleBuffering::Disabled));
    EXPECT_THAT(swapChain, HandleIsValid());

    for (uint32_t i = 0; i < 5; ++i)
    {
        swapChain.recreate(*device_, *physicalDevice_, context_->getSurface());
    }
    EXPECT_THAT(swapChain, HandleIsValid());
    EXPECT_EQ(swapChain.getImageCount(), 2u);
}

TEST_F(VulkanSwapChainTest, WindowResized)
{
    VulkanSwapChain swapChain(createSwapChain(TripleBuffering::Disabled));
    EXPECT_THAT(swapChain, HandleIsValid());
    EXPECT_EQ(swapChain.getExtent().width, kDefaultWidth);
    EXPECT_EQ(swapChain.getExtent().height, kDefaultHeight);

    constexpr uint32_t kNewWidth = 512;
    constexpr uint32_t kNewHeight = 1024;
    window_->setSize(kNewWidth, kNewHeight);
    swapChain.recreate(*device_, *physicalDevice_, context_->getSurface());

    EXPECT_THAT(swapChain, HandleIsValid());
    EXPECT_EQ(swapChain.getExtent().width, kNewWidth);
    EXPECT_EQ(swapChain.getExtent().height, kNewHeight);

    window_->setSize(kDefaultWidth, kDefaultHeight);
}
} // namespace
} // namespace crisp::test