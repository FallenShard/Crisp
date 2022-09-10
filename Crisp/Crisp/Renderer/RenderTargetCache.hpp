#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

#include <optional>

namespace crisp
{

class RenderTargetBuilder
{
public:
    RenderTargetBuilder() = default;

    RenderTargetBuilder& setFormat(VkFormat format, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);
    RenderTargetBuilder& setLayerAndMipLevelCount(uint32_t layerCount = 1, uint32_t mipLevelCount = 1);
    RenderTargetBuilder& setDepthSliceCount(uint32_t depthSlices = 1);
    RenderTargetBuilder& setCreateFlags(VkImageCreateFlags createFlags);
    RenderTargetBuilder& setBuffered(bool isBuffered);
    RenderTargetBuilder& setSize(VkExtent2D size, bool isSwapChainDependent);
    RenderTargetBuilder& configureColorRenderTarget(
        VkImageUsageFlags usageFlags, VkClearColorValue clearValue = {0, 0, 0, 0});
    RenderTargetBuilder& configureDepthRenderTarget(
        VkImageUsageFlags usageFlags, VkClearDepthStencilValue clearValue = {0.0f, 0});

    std::unique_ptr<RenderTarget> create(const VulkanDevice& device);

private:
    RenderTargetInfo m_info;
};

class RenderTargetCache
{
public:
    RenderTargetCache() = default;

    RenderTarget* addRenderTarget(std::string&& name, std::unique_ptr<RenderTarget> renderTarget);

    void resizeRenderTargets(const VulkanDevice& device, VkExtent2D swapChainExtent);

    RenderTarget* get(std::string&& name) const;

    std::unique_ptr<RenderTarget> extract(std::string&& name);

    void transitionInitialLayouts();

private:
    FlatHashMap<std::string, std::unique_ptr<RenderTarget>> m_renderTargets;
};

} // namespace crisp