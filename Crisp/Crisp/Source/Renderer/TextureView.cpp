#include "TextureView.hpp"

#include "Renderer/Texture.hpp"
#include "Renderer/VulkanRenderer.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanDevice.hpp"

namespace crisp
{
    TextureView::TextureView(VulkanRenderer* renderer, Texture* texture, VkImageViewType type, uint32_t baseLayer, uint32_t numLayers, uint32_t baseMipLevel, uint32_t mipLevels)
        : m_renderer(renderer)
        , m_texture(texture)
    {
        m_imageView = texture->getImage()->createView(type, baseLayer, numLayers, baseMipLevel, mipLevels);
    }

    TextureView::~TextureView()
    {
        vkDestroyImageView(m_renderer->getDevice()->getHandle(), m_imageView, nullptr);
    }

    VkImageView TextureView::getHandle() const
    {
        return m_imageView;
    }

    VkDescriptorImageInfo TextureView::getDescriptorInfo(VkSampler sampler, VkImageLayout layout) const
    {
        return { sampler, m_imageView, layout };
    }
}