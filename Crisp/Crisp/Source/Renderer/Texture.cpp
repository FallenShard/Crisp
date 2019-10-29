#include "Texture.hpp"

#include "Renderer/Renderer.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanBuffer.hpp"

namespace crisp
{
    Texture::Texture(Renderer* renderer, VkExtent3D extent, uint32_t numLayers, VkFormat format,
        VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageCreateFlags createFlags)
        : m_renderer(renderer)
    {
        m_image = std::make_unique<VulkanImage>(renderer->getDevice(), extent, numLayers, 1, format, usage, aspect, createFlags);
    }

    Texture::Texture(Renderer* renderer, VkExtent3D extent, uint32_t numLayers, uint32_t numMipmaps, VkFormat format,
        VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageCreateFlags createFlags)
        : m_renderer(renderer)
    {
        m_image = std::make_unique<VulkanImage>(renderer->getDevice(), extent, numLayers, numMipmaps, format, usage, aspect, createFlags);
    }

    Texture::~Texture()
    {
    }

    VulkanImage* Texture::getImage() const
    {
        return m_image.get();
    }

    void Texture::transitionLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout, uint32_t baseLayer, uint32_t numLayers, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
    {
        m_image->transitionLayout(cmdBuffer, newLayout, baseLayer, numLayers, srcStage, dstStage);
    }

    void Texture::updateFromStaging(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, uint32_t baseLayer, uint32_t numLayers)
    {
        m_image->copyFrom(commandBuffer, buffer, baseLayer, numLayers);
    }

    void Texture::fill(const void* data, VkDeviceSize size)
    {
        auto buffer = std::make_shared<VulkanBuffer>(m_renderer->getDevice(), size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        buffer->updateFromHost(data, size, 0);

        m_renderer->enqueueResourceUpdate([this, buffer](VkCommandBuffer cmdBuffer)
        {
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            m_image->copyFrom(cmdBuffer, *buffer, 0, 1);

            if (m_image->getMipLevels() > 1)
                m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            for (uint32_t i = 1; i < m_image->getMipLevels(); i++)
            {
                VkImageBlit imageBlit = {};

                imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                imageBlit.srcSubresource.baseArrayLayer = 0;
                imageBlit.srcSubresource.layerCount     = 1;
                imageBlit.srcSubresource.mipLevel       = i - 1;
                imageBlit.srcOffsets[1].x = int32_t(m_image->getWidth()  >> (i - 1));
                imageBlit.srcOffsets[1].y = int32_t(m_image->getHeight() >> (i - 1));
                imageBlit.srcOffsets[1].z = 1;

                imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                imageBlit.dstSubresource.baseArrayLayer = 0;
                imageBlit.dstSubresource.layerCount     = 1;
                imageBlit.dstSubresource.mipLevel       = i;
                imageBlit.dstOffsets[1].x = int32_t(m_image->getWidth()  >> i);
                imageBlit.dstOffsets[1].y = int32_t(m_image->getHeight() >> i);
                imageBlit.dstOffsets[1].z = 1;

                VkImageSubresourceRange mipRange = {};
                mipRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                mipRange.baseMipLevel   = i;
                mipRange.levelCount     = 1;
                mipRange.baseArrayLayer = 0;
                mipRange.layerCount     = 1;

                m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                vkCmdBlitImage(cmdBuffer, m_image->getHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    m_image->getHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
                m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            }

            VkImageSubresourceRange mipRange = {};
            mipRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            mipRange.baseMipLevel   = 0;
            mipRange.levelCount     = m_image->getMipLevels();
            mipRange.baseArrayLayer = 0;
            mipRange.layerCount     = 1;
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);


            m_renderer->scheduleBufferForRemoval(buffer);
        });
    }

    void Texture::fill(const void* data, VkDeviceSize size, uint32_t baseLayer, uint32_t numLayers)
    {
        auto buffer = std::make_shared<VulkanBuffer>(m_renderer->getDevice(), size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        buffer->updateFromHost(data, size, 0);

        m_renderer->enqueueResourceUpdate([this, buffer, baseLayer, numLayers](VkCommandBuffer cmdBuffer)
        {
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, baseLayer, numLayers, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            m_image->copyFrom(cmdBuffer, *buffer, baseLayer, numLayers);
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, baseLayer, numLayers, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            m_renderer->scheduleBufferForRemoval(buffer);
        });
    }

    void Texture::fill(const VulkanBuffer& buffer, VkDeviceSize /*size*/)
    {
        m_renderer->enqueueResourceUpdate([this, &buffer](VkCommandBuffer cmdBuffer)
        {
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
            m_image->copyFrom(cmdBuffer, buffer, 0, 1);
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        });
    }

    void Texture::fill(const VulkanBuffer& buffer, VkDeviceSize /*size*/, uint32_t baseLayer, uint32_t numLayers)
    {
        m_renderer->enqueueResourceUpdate([this, &buffer, baseLayer, numLayers](VkCommandBuffer cmdBuffer)
        {
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, baseLayer, numLayers, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            m_image->copyFrom(cmdBuffer, buffer, baseLayer, numLayers);
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, baseLayer, numLayers, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
    }

    std::unique_ptr<VulkanImageView> Texture::createView(VkImageViewType type, uint32_t baseLayer, uint32_t numLayers, uint32_t baseMipLevel, uint32_t mipLevels)
    {
        return m_image->createView(type, baseLayer, numLayers, baseMipLevel, mipLevels);
    }
}