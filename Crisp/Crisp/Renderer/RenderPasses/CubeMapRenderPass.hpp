#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
class CubeMapRenderPass : public VulkanRenderPass
{
public:
    CubeMapRenderPass(Renderer& renderer, VkExtent2D renderArea, bool allocateMipmaps = false);

    std::unique_ptr<VulkanImage> extractRenderTarget(uint32_t index);

protected:
    virtual void createResources(Renderer& renderer) override;

    bool m_allocateMipmaps;
};
} // namespace crisp
