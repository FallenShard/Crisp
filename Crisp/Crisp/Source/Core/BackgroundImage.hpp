#pragma once

#include <memory>
#include <string>

#include <vulkan/vulkan.h>

#include <Vesper/RayTracerUpdate.hpp>

#include "Renderer/DescriptorSetGroup.hpp"

namespace crisp
{
    class VulkanRenderer;
    class VulkanSampler;
    class Texture;
    class TextureView;

    class VulkanRenderPass;
    class FullScreenQuadPipeline;

    class BackgroundImage
    {
    public:
        BackgroundImage(std::string fileName, VkFormat format, VulkanRenderer* renderer, VulkanRenderPass* pass = nullptr);
        ~BackgroundImage();

        void resize(int width, int height);

        void draw();
        void drawDirect(VkCommandBuffer cmdBuffer);

    private:
        VulkanRenderer* m_renderer;

        std::unique_ptr<FullScreenQuadPipeline> m_pipeline;
        DescriptorSetGroup m_descSetGroup;

        std::unique_ptr<Texture> m_texture;
        std::unique_ptr<TextureView> m_textureView;
        
        VkExtent2D m_extent;
        std::unique_ptr<VulkanSampler> m_sampler;
        VkViewport m_viewport;
        float m_aspectRatio;
    };
}