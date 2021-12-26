#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
class ShadowPass final : public VulkanRenderPass
{
public:
    ShadowPass(Renderer& renderer, unsigned int shadowMapSize, unsigned int numCascades);

protected:
    virtual void createResources(Renderer& renderer) override;

    unsigned int m_numCascades;

    std::vector<std::vector<std::unique_ptr<VulkanImageView>>> m_individualLayerViews;
};
} // namespace crisp
