#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class TransmittanceLutPass : public VulkanRenderPass
    {
    public:
        TransmittanceLutPass(Renderer* renderer);

    protected:
        virtual void createResources() override;
    };

    class SkyViewLutPass : public VulkanRenderPass
    {
    public:
        SkyViewLutPass(Renderer* renderer, VkExtent2D renderArea);

    protected:
        virtual void createResources() override;
    };

    class CameraVolumesPass : public VulkanRenderPass
    {
    public:
        CameraVolumesPass(Renderer* renderer, VkExtent2D renderArea);

    protected:
        virtual void createResources() override;
    };

    class RayMarchingPass : public VulkanRenderPass
    {
    public:
        RayMarchingPass(Renderer* renderer, VkExtent2D renderArea);

    protected:
        virtual void createResources() override;
    };
}
