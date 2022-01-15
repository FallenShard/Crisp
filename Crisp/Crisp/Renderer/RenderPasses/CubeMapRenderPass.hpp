#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
// class CubeMapRenderPass : public VulkanRenderPass
//{
// public:
//     CubeMapRenderPass(const VulkanDevice& device, VkExtent2D renderArea, bool allocateMipmaps = false);
//
//     std::unique_ptr<VulkanImage> extractRenderTarget(uint32_t index);
//
// protected:
//     virtual void createResources(const VulkanDevice& device) override;
//
//     bool m_allocateMipmaps;
// };

std::unique_ptr<VulkanRenderPass> createCubeMapPass(const VulkanDevice& device, VkExtent2D renderArea,
    bool allocateMipmaps = false);

} // namespace crisp
