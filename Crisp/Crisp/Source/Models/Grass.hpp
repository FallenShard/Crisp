#pragma once

#include <memory>
#include <vector>

#include "Renderer/DrawCommand.hpp"

namespace crisp
{
    class Renderer;
    class VulkanPipeline;
    class Material;
    class Geometry;
    class VulkanRenderPass;
    class UniformBuffer;

    class CascadedShadowMapper;
    class VulkanSampler;

    class Grass
    {
    public:
        Grass(Renderer* renderer, VulkanRenderPass* mainRenderPass, VulkanRenderPass* renderPass, CascadedShadowMapper* csm, UniformBuffer* cameraBuffer, VulkanSampler* sampler);
        ~Grass();

        DrawCommand createDrawCommand();
        DrawCommand createShadowCommand(uint32_t subpassIndex);

    private:
        Renderer* m_renderer;

        std::unique_ptr<Geometry> m_bladeGeometry;


        std::unique_ptr<VulkanPipeline> m_pipeline;
        std::unique_ptr<Material>       m_material;

        std::vector<std::unique_ptr<VulkanPipeline>> m_shadowPipelines;
        std::unique_ptr<Material> m_shadowMaterial;

        DrawCommand m_drawCommand;
        DrawCommand m_shadowDrawCommand;
    };
}