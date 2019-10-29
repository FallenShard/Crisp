#pragma once

#include <memory>

#include <CrispCore/Math/Headers.hpp>

#include "Renderer/DrawCommand.hpp"
#include "Renderer/RenderNode.hpp"
#include "Geometry/TransformPack.hpp"

namespace crisp
{
    class Renderer;
    class VulkanRenderPass;
    class VulkanPipeline;
    class VulkanDevice;
    class VulkanImageView;
    class VulkanSampler;

    class Material;
    class Texture;
    class UniformBuffer;
    class Geometry;

    class Skybox
    {
    public:
        Skybox(Renderer* renderer, const VulkanRenderPass& renderPass, const std::string& cubeMapFolder);
        Skybox(Renderer* renderer, const VulkanRenderPass& renderPass, std::unique_ptr<VulkanImage> image, std::unique_ptr<VulkanImageView> imageView);
        ~Skybox();

        void updateTransforms(const glm::mat4& V, const glm::mat4& P);

        DrawCommand createDrawCommand() const;
        RenderNode createRenderNode();

        VulkanImageView* getSkyboxView() const;

    private:
        Renderer* m_renderer;
        VulkanDevice*   m_device;

        std::unique_ptr<Geometry> m_cubeGeometry;

        std::unique_ptr<VulkanPipeline> m_pipeline;
        std::unique_ptr<Material> m_material;

        TransformPack m_transformPack;
        std::unique_ptr<UniformBuffer> m_transformBuffer;

        std::unique_ptr<VulkanImage> m_cubeMap;
        std::unique_ptr<VulkanImageView> m_cubeMapView;
        std::unique_ptr<VulkanSampler>   m_sampler;
    };
}