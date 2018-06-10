#pragma once

#include <memory>

#include "Math/Headers.hpp"
#include "Renderer/VertexBufferBindingGroup.hpp"
#include "Renderer/DescriptorSetGroup.hpp"

namespace crisp
{
    class Renderer;
    class VulkanPipeline;
    class VulkanDevice;
    class VulkanRenderPass;
    class VulkanImageView;
    class VulkanSampler;

    class Texture;
    class UniformBuffer;
    class MeshGeometry;

    class Skybox
    {
    public:
        Skybox(Renderer* renderer, VulkanRenderPass* renderPass, const std::string& cubeMapFolder);
        ~Skybox();

        void updateTransforms(const glm::mat4& P, const glm::mat4& V);

        void updateDeviceBuffers(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex);
        void draw(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex) const;

        VulkanImageView* getSkyboxView() const;

    private:
        Renderer* m_renderer;
        VulkanDevice*   m_device;

        std::unique_ptr<MeshGeometry> m_cubeGeometry;

        std::unique_ptr<VulkanPipeline> m_pipeline;
        DescriptorSetGroup m_descriptorSetGroup;

        struct Transforms
        {
            glm::mat4 MVP;
            glm::mat4 MV;
            glm::mat4 M;
            glm::mat4 N;
        };

        Transforms m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;

        std::unique_ptr<Texture>         m_cubeMap;
        std::unique_ptr<VulkanImageView> m_cubeMapView;
        std::unique_ptr<VulkanSampler>   m_sampler;
    };
}