#pragma once

#include <memory>

#include "Math/Headers.hpp"
#include "Renderer/VertexBufferBindingGroup.hpp"
#include "Renderer/DescriptorSetGroup.hpp"

namespace crisp
{
    class VulkanRenderer;
    class VulkanDevice;
    class VulkanRenderPass;

    class Texture;
    class TextureView;
    class UniformBuffer;
    class IndexBuffer;
    class SkyboxPipeline;

    class Skybox
    {
    public:
        Skybox(VulkanRenderer* renderer, VulkanRenderPass* renderPass);
        ~Skybox();

        void updateTransforms(const glm::mat4& P, const glm::mat4& V);

        void updateDeviceBuffers(VkCommandBuffer& cmdBuffer, uint32_t currentFrameIndex);
        void draw(VkCommandBuffer& cmdBuffer, uint32_t currentFrameIndex, uint32_t subpassIndex) const;

        VkImageView getSkyboxView() const;

    private:
        VulkanRenderer* m_renderer;
        VulkanDevice* m_device;

        VertexBufferBindingGroup m_vertexBufferGroup;
        std::unique_ptr<VertexBuffer> m_buffer;
        std::unique_ptr<IndexBuffer> m_indexBuffer;

        std::unique_ptr<SkyboxPipeline> m_pipeline;
        std::unique_ptr<SkyboxPipeline> m_nextPipeline;
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

        std::unique_ptr<Texture> m_texture;
        std::unique_ptr<TextureView> m_textureView;
        VkSampler m_sampler;
    };
}