#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
class VarianceShadowMapPass : public VulkanRenderPass
{
public:
    VarianceShadowMapPass(Renderer* renderer, unsigned int shadowMapSize);

protected:
    virtual void createResources() override;

    std::vector<std::vector<std::unique_ptr<VulkanImageView>>> m_individualLayerViews;
};
} // namespace crisp
