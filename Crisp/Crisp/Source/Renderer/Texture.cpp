#include "Texture.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/TextureView.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"

namespace crisp
{
    Texture::Texture(VulkanRenderer* renderer, VkExtent3D extent, uint32_t numLayers, VkFormat format,
        VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageCreateFlags createFlags)
        : m_renderer(renderer)
    {
        m_image = std::make_unique<VulkanImage>(renderer->getDevice(), extent, numLayers, format, usage, aspect, createFlags);
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
        buffer->updateFromHost(data, size);

        m_renderer->enqueueResourceUpdate([this, buffer](VkCommandBuffer cmdBuffer)
        {
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            m_image->copyFrom(cmdBuffer, *buffer, 0, 1);
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            m_renderer->scheduleBufferForRemoval(buffer);
        });
    }

    void Texture::fill(const void* data, VkDeviceSize size, uint32_t baseLayer, uint32_t numLayers)
    {
        auto buffer = std::make_shared<VulkanBuffer>(m_renderer->getDevice(), size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        buffer->updateFromHost(data, size);

        m_renderer->enqueueResourceUpdate([this, buffer, baseLayer, numLayers](VkCommandBuffer cmdBuffer)
        {
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, baseLayer, numLayers, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            m_image->copyFrom(cmdBuffer, *buffer, baseLayer, numLayers);
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, baseLayer, numLayers, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            m_renderer->scheduleBufferForRemoval(buffer);
        });
    }

    void Texture::fill(const VulkanBuffer& buffer, VkDeviceSize size)
    {
        m_renderer->enqueueResourceUpdate([this, &buffer](VkCommandBuffer cmdBuffer)
        {
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
            m_image->copyFrom(cmdBuffer, buffer, 0, 1);
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        });
    }

    void Texture::fill(const VulkanBuffer& buffer, VkDeviceSize size, uint32_t baseLayer, uint32_t numLayers)
    {
        m_renderer->enqueueResourceUpdate([this, &buffer, baseLayer, numLayers](VkCommandBuffer cmdBuffer)
        {
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, baseLayer, numLayers, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
            m_image->copyFrom(cmdBuffer, buffer, baseLayer, numLayers);
            m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, baseLayer, numLayers, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        });
    }

    std::unique_ptr<TextureView> Texture::createView(VkImageViewType type, uint32_t baseLayer, uint32_t numLayers)
    {
        return std::make_unique<TextureView>(m_renderer, this, type, baseLayer, numLayers);
    }
}