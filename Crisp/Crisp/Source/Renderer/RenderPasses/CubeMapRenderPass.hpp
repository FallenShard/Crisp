#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class CubeMapRenderPass : public VulkanRenderPass
    {
    public:
        CubeMapRenderPass(Renderer* renderer, VkExtent2D renderArea);

        std::unique_ptr<VulkanImage> extractRenderTarget(uint32_t index);

    protected:
        virtual void createResources() override;
    };
}
