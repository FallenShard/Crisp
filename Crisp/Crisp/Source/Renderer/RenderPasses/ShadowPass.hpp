#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class ShadowPass : public VulkanRenderPass
    {
    public:
        ShadowPass(VulkanRenderer* renderer, unsigned int shadowMapSize, unsigned int numCascades);

    protected:
        virtual void createResources() override;

        unsigned int m_numCascades;
    };
}
