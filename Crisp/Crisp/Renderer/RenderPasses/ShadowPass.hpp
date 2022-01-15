#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
// class ShadowPass final : public VulkanRenderPass
//{
// public:
//     ShadowPass(const VulkanDevice& device, unsigned int shadowMapSize, unsigned int numCascades);
//
// protected:
//     virtual void createResources(const VulkanDevice& device) override;
//
//     unsigned int m_numCascades;
//
//     std::vector<std::vector<std::unique_ptr<VulkanImageView>>> m_individualLayerViews;
// };

std::unique_ptr<VulkanRenderPass> createShadowMapPass(const VulkanDevice& device, uint32_t shadowMapSize,
    uint32_t cascadeCount);

} // namespace crisp
