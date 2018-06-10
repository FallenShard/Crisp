#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include <Vesper/RayTracerUpdate.hpp>

#include "Renderer/DescriptorSetGroup.hpp"

namespace crisp
{
    class Renderer;
    class VulkanDevice;
    class VulkanBuffer;
    class VulkanSampler;
    class Texture;
    class VulkanImageView;
    class VulkanPipeline;

    class RayTracedImage
    {
    public:
        RayTracedImage(uint32_t width, uint32_t height, VkFormat format, Renderer* renderer);
        ~RayTracedImage();

        void postTextureUpdate(vesper::RayTracerUpdate rayTracerUpdate);

        void draw();
        void resize(int width, int height);

    private:
        Renderer* m_renderer;
        VulkanDevice*   m_device;

        std::unique_ptr<VulkanPipeline> m_pipeline;
        DescriptorSetGroup m_descSets;

        VkExtent3D m_extent;
        uint32_t   m_numChannels;

        std::vector<std::pair<unsigned int, vesper::RayTracerUpdate>> m_textureUpdates;
        std::unique_ptr<VulkanBuffer> m_stagingBuffer;
        unsigned int m_updatedImageIndex;

        std::unique_ptr<Texture>         m_texture;
        std::unique_ptr<VulkanImageView> m_VulkanImageView;
        std::unique_ptr<VulkanSampler>   m_sampler;

        VkViewport   m_viewport;
    };
}