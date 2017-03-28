#pragma once

#include <memory>
#include <string>
#include <array>
#include <list>

#include <vulkan/vulkan.h>

#include <Vesper/RayTracerUpdate.hpp>

#include "vulkan/DescriptorSetGroup.hpp"

namespace crisp
{
    class Image;
    class VulkanRenderer;
    class VulkanDevice;
    class VulkanSwapChain;
    class VulkanRenderPass;

    class FullScreenQuadPipeline;

    class Picture
    {
    public:
        Picture(uint32_t width, uint32_t height, VkFormat format, VulkanRenderer& renderer);
        ~Picture();

        void postTextureUpdate(vesper::RayTracerUpdate rayTracerUpdate);

        void draw();
        void resize(int width, int height);

    private:
        void recalculateViewport(int screenWidth, int screenHeight);

        VulkanRenderer* m_renderer;
        VulkanDevice*   m_device;

        std::unique_ptr<FullScreenQuadPipeline> m_pipeline;
        DescriptorSetGroup m_descSets;

        VkExtent2D m_extent;
        uint32_t   m_numChannels;

        std::vector<std::pair<unsigned int, vesper::RayTracerUpdate>> m_textureUpdates;
        VkBuffer     m_stagingTexBuffer;
        unsigned int m_updatedImageIndex;

        VkImage      m_imageArray;
        VkImageView  m_imageArrayView;
        VkSampler    m_sampler;

        VkViewport   m_viewport;
    };
}