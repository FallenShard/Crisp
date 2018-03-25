#pragma once

#include <memory>
#include <vulkan/vulkan.h>

namespace crisp
{
    class Texture;
    class VulkanRenderer;

    class TextureView
    {
    public:
        TextureView(VulkanRenderer* m_renderer, Texture* texture, VkImageViewType type, uint32_t baseLayer, uint32_t numLayers, uint32_t baseMipLevel, uint32_t mipLevels);
        ~TextureView();

        VkImageView getHandle() const;

        VkDescriptorImageInfo getDescriptorInfo(VkSampler sampler = VK_NULL_HANDLE, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const;

    private:
        VulkanRenderer* m_renderer;
        Texture* m_texture;

        VkImageView m_imageView;
    };
}