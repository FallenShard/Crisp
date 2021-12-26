#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

#include <CrispCore/RobinHood.hpp>

namespace crisp
{
class DefaultRenderPass : public VulkanRenderPass
{
public:
    DefaultRenderPass(Renderer& renderer);

    void recreateFramebuffer(Renderer& renderer, VkImageView VulkanSwapChainImageView);

private:
    virtual void createResources(Renderer& renderer) override;

    robin_hood::unordered_flat_map<VkImageView, std::unique_ptr<VulkanFramebuffer>> m_imageFramebuffers;
};
} // namespace crisp