#include <Crisp/Vulkan/Test/VulkanTest.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>
#include <Crisp/Vulkan/VulkanSwapChain.hpp>

#include <Crisp/Core/Result.hpp>

using namespace crisp;

class VulkanSwapChainTest : public VulkanTest
{
};

template <typename T>
struct VulkanSwapChainData
{
    T deps;
    VulkanSwapChain swapChain;

    VulkanSwapChainData(T&& deps, VulkanSwapChain&& swapChain)
        : deps(std::move(deps))
        , swapChain(std::move(swapChain))
    {
    }
};

template <int Width, int Height>
auto createSwapChain(TripleBuffering tripleBuffering)
{
    struct
    {
        std::unique_ptr<Window> window{std::make_unique<Window>(
            glm::ivec2{0, 0}, glm::ivec2{Width, Height}, "unit_test", WindowVisibility::Hidden)};
        std::unique_ptr<VulkanContext> context{std::make_unique<VulkanContext>(
            window->createSurfaceCallback(), ApplicationEnvironment::getRequiredVulkanInstanceExtensions(), false)};
        std::unique_ptr<VulkanPhysicalDevice> physicalDevice{std::make_unique<VulkanPhysicalDevice>(
            context->selectPhysicalDevice(createDefaultDeviceExtensions()).unwrap())};
        std::unique_ptr<VulkanDevice> device{std::make_unique<VulkanDevice>(
            *physicalDevice,
            createDefaultQueueConfiguration(*context, *physicalDevice),
            RendererConfig::VirtualFrameCount)};
    } deps;

    VulkanSwapChain swapChain(*deps.device, *deps.physicalDevice, deps.context->getSurface(), tripleBuffering);
    return VulkanSwapChainData(std::move(deps), std::move(swapChain));
}

TEST_F(VulkanSwapChainTest, Constructor)
{
    auto [deps, swapChain] = createSwapChain<200, 300>(TripleBuffering::Enabled);
    ASSERT_NE(swapChain.getHandle(), nullptr);

    // Self-move assign
    swapChain = std::move(swapChain);
    ASSERT_NE(swapChain.getHandle(), nullptr);
}

TEST_F(VulkanSwapChainTest, Bounds)
{
    auto [deps, swapChain] = createSwapChain<200, 300>(TripleBuffering::Enabled);
    ASSERT_NE(swapChain.getHandle(), nullptr);

    ASSERT_EQ(swapChain.getImageFormat(), VK_FORMAT_B8G8R8A8_UNORM);
    ASSERT_EQ(swapChain.getExtent().width, 200u);
    ASSERT_EQ(swapChain.getExtent().height, 300u);

    const auto extent = swapChain.getExtent();
    const auto viewport = swapChain.getViewport();
    ASSERT_EQ(extent.width, viewport.width);
    ASSERT_EQ(extent.height, viewport.height);
    ASSERT_EQ(viewport.x, 0);
    ASSERT_EQ(viewport.y, 0);
    ASSERT_EQ(viewport.minDepth, 0);
    ASSERT_EQ(viewport.maxDepth, 1);

    const auto scissor = swapChain.getScissorRect();
    ASSERT_EQ(extent.width, scissor.extent.width);
    ASSERT_EQ(extent.height, scissor.extent.height);
    ASSERT_EQ(scissor.offset.x, 0);
    ASSERT_EQ(scissor.offset.y, 0);
}

TEST_F(VulkanSwapChainTest, SwapImagesTripleBuffering)
{
    auto [deps, swapChain] = createSwapChain<200, 300>(TripleBuffering::Enabled);
    ASSERT_NE(swapChain.getHandle(), nullptr);

    ASSERT_EQ(swapChain.getSwapChainImageCount(), 3u);
}

TEST_F(VulkanSwapChainTest, SwapImagesAreDifferent)
{
    auto [deps, swapChain] = createSwapChain<200, 300>(TripleBuffering::Disabled);
    ASSERT_NE(swapChain.getHandle(), nullptr);

    ASSERT_EQ(swapChain.getSwapChainImageCount(), 2u);
    ASSERT_NE(swapChain.getImageView(0), swapChain.getImageView(1));
}

TEST_F(VulkanSwapChainTest, Recreate)
{
    auto [deps, swapChain] = createSwapChain<100, 300>(TripleBuffering::Disabled);
    ASSERT_NE(swapChain.getHandle(), nullptr);

    for (uint32_t i = 0; i < 5; ++i)
        swapChain.recreate(*deps.device, *deps.physicalDevice, deps.context->getSurface());
    ASSERT_NE(swapChain.getHandle(), nullptr);
    ASSERT_EQ(swapChain.getSwapChainImageCount(), 2u);
}

TEST_F(VulkanSwapChainTest, WindowResized)
{
    auto [deps, swapChain] = createSwapChain<150, 300>(TripleBuffering::Disabled);
    ASSERT_NE(swapChain.getHandle(), nullptr);
    EXPECT_EQ(swapChain.getExtent().width, 150u);
    EXPECT_EQ(swapChain.getExtent().height, 300u);

    deps.window->setSize(512, 1024);
    swapChain.recreate(*deps.device, *deps.physicalDevice, deps.context->getSurface());

    ASSERT_NE(swapChain.getHandle(), nullptr);
    EXPECT_EQ(swapChain.getExtent().width, 512);
    EXPECT_EQ(swapChain.getExtent().height, 1024);
}
