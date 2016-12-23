#pragma once

#include <memory>
#include <string>
#include <array>
#include <list>

#include <vulkan/vulkan.h>

#include <Vesper/ImageBlockEventArgs.hpp>

#include "Vulkan/DrawItem.hpp"

namespace crisp
{
    class Image;
    class VulkanRenderer;
    class VulkanSwapChain;
    class VulkanRenderPass;

    class FullScreenQuadPipeline;

    class Picture
    {
    public:
        Picture(uint32_t width, uint32_t height, VkFormat format, VulkanRenderer& renderer);
        ~Picture();

        void updateTexture(vesper::ImageBlockEventArgs imageBlockArgs);

        void draw();
        void resize();

    private:
        VulkanRenderer* m_renderer;

        std::unique_ptr<FullScreenQuadPipeline> m_pipeline;
        VkDescriptorSet m_descriptorSet;

        VkBuffer m_vertexBuffer;
        VkBuffer m_indexBuffer;

        VkImage m_tex;
        VkImageView m_texView;
        VkSampler m_vkSampler;

        DrawItem m_drawItem;

        VkExtent2D m_extent;

        std::vector<std::pair<unsigned int, vesper::ImageBlockEventArgs>> m_textureUpdates;
        VkBuffer m_stagingTexBuffer;
        unsigned int m_updatedImageIndex;
    };
}