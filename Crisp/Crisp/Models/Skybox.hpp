#pragma once

#include <memory>

#include <Crisp/Math/Headers.hpp>

#include <Crisp/Geometry/TransformPack.hpp>
#include <Crisp/Renderer/DrawCommand.hpp>
#include <Crisp/Renderer/RenderNode.hpp>

namespace crisp
{
class Renderer;
class VulkanRenderPass;
class VulkanPipeline;
class VulkanDevice;
class VulkanImageView;
class VulkanSampler;

class Material;
class UniformBuffer;
class Geometry;

class Skybox
{
public:
    Skybox(Renderer* renderer, const VulkanRenderPass& renderPass, const std::string& cubeMapFolder);
    Skybox(
        Renderer* renderer,
        const VulkanRenderPass& renderPass,
        const VulkanImageView& cubeMapView,
        const VulkanSampler& sampler);
    ~Skybox();

    void updateTransforms(const glm::mat4& V, const glm::mat4& P);

    const RenderNode& getRenderNode();

    VulkanImageView* getSkyboxView() const;

private:
    void updateRenderNode(const VulkanSampler& sampler, const VulkanImageView& cubeMapView);

    std::unique_ptr<Geometry> m_cubeGeometry;

    std::unique_ptr<VulkanPipeline> m_pipeline;
    std::unique_ptr<Material> m_material;

    TransformPack m_transformPack;
    std::unique_ptr<UniformBuffer> m_transformBuffer;

    std::unique_ptr<VulkanImage> m_cubeMap;
    std::unique_ptr<VulkanImageView> m_cubeMapView;
    std::unique_ptr<VulkanSampler> m_sampler;

    RenderNode m_renderNode;
};
} // namespace crisp