#pragma once

#include <memory>
#include <string>
#include <array>
#include <list>

#include <vulkan/vulkan.h>

#include <Vesper/RayTracerUpdate.hpp>

#include "Vulkan/DrawItem.hpp"

namespace crisp
{
    class Image;
    class VulkanRenderer;
    class VulkanSwapChain;
    class VulkanRenderPass;

    class FullScreenQuadPipeline;

    class StaticPicture
    {
    public:
        StaticPicture(std::string fileName, VkFormat format, VulkanRenderer& renderer);
        ~StaticPicture();

        void resize(int width, int height);

        void draw();

    private:
        VulkanRenderer* m_renderer;

        std::unique_ptr<FullScreenQuadPipeline> m_pipeline;
        VkDescriptorSet m_descriptorSet;

        VkBuffer m_vertexBuffer;
        VkBuffer m_indexBuffer;

        VkExtent2D m_extent;
        VkImage m_tex;
        VkImageView m_texView;
        VkSampler m_vkSampler;

        VkViewport m_viewport;
        float m_aspectRatio;

        DrawItem m_drawItem;
    };
}