#pragma once

#include <memory>

#include "Math/Headers.hpp"
#include "vulkan/VertexBufferBindingGroup.hpp"
#include "vulkan/DescriptorSetGroup.hpp"

namespace crisp
{
    class VulkanRenderer;
    class VulkanDevice;
    class VulkanRenderPass;

    class IndexBuffer;
    class SkyboxPipeline;

    class Skybox
    {
    public:
        Skybox(VulkanRenderer* renderer, VulkanRenderPass* renderPass);
        ~Skybox();

        void updateTransforms(const glm::mat4& P, const glm::mat4& V);

        void resize(int width, int height);

        void updateDeviceBuffers(VkCommandBuffer& cmdBuffer, uint32_t currentFrameIndex);
        void draw(VkCommandBuffer& cmdBuffer, uint32_t currentFrameIndex) const;

    private:
        VulkanRenderer* m_renderer;
        VulkanDevice* m_device;

        VertexBufferBindingGroup m_vertexBufferGroup;
        std::unique_ptr<VertexBuffer> m_buffer;
        std::unique_ptr<IndexBuffer> m_indexBuffer;

        std::unique_ptr<SkyboxPipeline> m_pipeline;
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

        VkImage m_texture;
        VkImageView m_imageView;
        VkSampler m_sampler;
    };
}