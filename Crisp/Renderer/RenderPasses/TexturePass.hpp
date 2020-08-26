#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class TexturePass : public VulkanRenderPass
    {
    public:
        TexturePass(Renderer* renderer, VkExtent2D renderArea);

        std::unique_ptr<VulkanImage> extractRenderTarget(uint32_t index);

    protected:
        virtual void createResources() override;
    };
}
