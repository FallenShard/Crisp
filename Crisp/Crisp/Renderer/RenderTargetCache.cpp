#include <Crisp/Renderer/RenderTargetCache.hpp>

#include <Crisp/Core/Checks.hpp>

namespace crisp {
RenderTargetBuilder& RenderTargetBuilder::setFormat(const VkFormat format, const VkSampleCountFlagBits sampleCount) {
    m_info.format = format;
    m_info.sampleCount = sampleCount;
    return *this;
}

RenderTargetBuilder& RenderTargetBuilder::setLayerAndMipLevelCount(
    const uint32_t layerCount, const uint32_t mipLevelCount) {
    m_info.layerCount = layerCount;
    m_info.mipmapCount = mipLevelCount;
    return *this;
}

RenderTargetBuilder& RenderTargetBuilder::setDepthSliceCount(const uint32_t depthSliceCount) {
    m_info.depthSlices = depthSliceCount;
    return *this;
}

RenderTargetBuilder& RenderTargetBuilder::setCreateFlags(VkImageCreateFlags createFlags) {
    m_info.createFlags = createFlags;
    return *this;
}

RenderTargetBuilder& RenderTargetBuilder::setSize(const VkExtent2D size, const bool isSwapChainDependent) {
    m_info.size = size;
    m_info.isSwapChainDependent = isSwapChainDependent;
    return *this;
}

RenderTargetBuilder& RenderTargetBuilder::configureColorRenderTarget(
    VkImageUsageFlags usageFlags, VkClearColorValue clearValue) {
    m_info.initSrcStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    m_info.initDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    m_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | usageFlags;
    m_info.clearValue.color = clearValue;
    return *this;
}

RenderTargetBuilder& RenderTargetBuilder::configureDepthRenderTarget(
    VkImageUsageFlags usageFlags, VkClearDepthStencilValue clearValue) {
    m_info.initSrcStageFlags = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    if (usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) {
        m_info.initDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        m_info.initDstStageFlags =
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    }
    m_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | usageFlags;
    m_info.clearValue.depthStencil = clearValue;
    return *this;
}

std::unique_ptr<RenderTarget> RenderTargetBuilder::create(const VulkanDevice& device) {
    const VkExtent3D dims{m_info.size.width, m_info.size.height, m_info.depthSlices};
    const uint32_t layerMultiplier = 1;
    const uint32_t totalLayerCount = m_info.layerCount * layerMultiplier;
    return std::make_unique<RenderTarget>(
        m_info,
        std::make_unique<VulkanImage>(
            device, dims, totalLayerCount, m_info.mipmapCount, m_info.format, m_info.usage, m_info.createFlags));
}

RenderTarget* RenderTargetCache::addRenderTarget(std::string&& name, std::unique_ptr<RenderTarget> renderTarget) {
    CRISP_CHECK(
        m_renderTargets.find(name) == m_renderTargets.end(),
        "Render target with name {} is already present in cache!",
        name);
    return m_renderTargets.emplace(std::move(name), std::move(renderTarget)).first->second.get();
}

void RenderTargetCache::resizeRenderTargets(const VulkanDevice& device, const VkExtent2D swapChainExtent) {
    for (auto& [name, renderTarget] : m_renderTargets) {
        auto& info = renderTarget->info;
        // If this render target does not depend on the swap chain size, no need to resize.
        if (!info.isSwapChainDependent) {
            continue;
        }

        info.size = swapChainExtent;
        const uint32_t layerMultiplier{1};
        const VkExtent3D dims{info.size.width, info.size.height, info.depthSlices};
        const uint32_t totalLayerCount{info.layerCount * layerMultiplier};
        renderTarget->image = std::make_unique<VulkanImage>(
            device, dims, totalLayerCount, info.mipmapCount, info.format, info.usage, info.createFlags);
    }
}

RenderTarget* RenderTargetCache::get(std::string&& name) const {
    return m_renderTargets.at(name).get();
}

std::unique_ptr<RenderTarget> RenderTargetCache::extract(std::string&& name) {
    auto iter = m_renderTargets.find(name);
    if (iter == m_renderTargets.end()) {
        return nullptr;
    }

    auto rt = std::move(iter->second);
    m_renderTargets.erase(name);
    return rt;
}

} // namespace crisp