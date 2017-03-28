#include "StaticPicture.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "IO/Image.hpp"

#include "Vulkan/VulkanRenderer.hpp"
#include "Vulkan/VulkanSwapChain.hpp"
#include "Vulkan/Pipelines/FullScreenQuadPipeline.hpp"

namespace crisp
{
    StaticPicture::StaticPicture(std::string fileName, VkFormat format, VulkanRenderer& renderer)
    {
        m_renderer = &renderer;
        m_pipeline = std::make_unique<FullScreenQuadPipeline>(m_renderer, &m_renderer->getDefaultRenderPass());

        std::shared_ptr<Image> image = std::make_shared<Image>(fileName);

        // create texture image
        unsigned int numChannels = 4;
        m_extent.width  = image->getWidth();
        m_extent.height = image->getHeight();
        m_aspectRatio = static_cast<float>(m_extent.width) / m_extent.height;
        auto byteSize = m_extent.width * m_extent.height * numChannels * sizeof(unsigned char);

        m_viewport.minDepth = 0;
        m_viewport.maxDepth = 1;
        resize(m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height);

        m_tex = renderer.getDevice().createDeviceImage(m_extent.width, m_extent.height, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        renderer.addCopyAction([this, byteSize, image, format](VkCommandBuffer& cmdBuffer)
        {
            auto device = &m_renderer->getDevice();
            auto stagingBufferInfo = device->createBuffer(device->getStagingBufferHeap(), byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            memcpy(static_cast<char*>(device->getStagingMemoryPtr()) + stagingBufferInfo.second.offset, image->getData(), static_cast<size_t>(byteSize));

            VkImageMemoryBarrier barrier = {};
            barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout                       = VK_IMAGE_LAYOUT_PREINITIALIZED;
            barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcAccessMask                   = VK_ACCESS_HOST_WRITE_BIT;
            barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.image                           = m_tex;
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = 1;
            barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;

            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset                    = 0;
            copyRegion.bufferRowLength                 = image->getWidth();
            copyRegion.bufferImageHeight               = image->getHeight();
            copyRegion.imageExtent                     = { image->getWidth(), image->getHeight() };
            copyRegion.imageOffset                     = { 0, 0, 0 };
            copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount     = 1;
            vkCmdCopyBufferToImage(cmdBuffer, stagingBufferInfo.first, m_tex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
            barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.image                           = m_tex;
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = 1;
            barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;

            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            m_renderer->scheduleStagingBufferForRemoval(stagingBufferInfo.first, stagingBufferInfo.second);

            // create view
            m_texView = device->createImageView(m_tex, VK_IMAGE_VIEW_TYPE_2D_ARRAY, format, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);

            // create sampler
            m_vkSampler = device->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

            // descriptor set
            m_descSetGroup =
            {
                m_pipeline->allocateDescriptorSet(0)
            };

            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView   = m_texView;
            imageInfo.sampler     = m_vkSampler;
            m_descSetGroup.postImageUpdate(0, 0, VK_DESCRIPTOR_TYPE_SAMPLER, imageInfo);
            m_descSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo);
            m_descSetGroup.flushUpdates(device);
        });
    }

    StaticPicture::~StaticPicture()
    {
        vkDestroySampler(m_renderer->getDevice().getHandle(), m_vkSampler, nullptr);
        vkDestroyImageView(m_renderer->getDevice().getHandle(), m_texView, nullptr);
    }

    void StaticPicture::resize(int width, int height)
    {
        m_pipeline->resize(width, height);

        float screenAspect = static_cast<float>(width) / height;
        if (screenAspect > m_aspectRatio)
        {
            m_viewport.height = static_cast<float>(height);
            m_viewport.width = static_cast<float>(height * m_aspectRatio);
            m_viewport.x = (width - m_viewport.width) / 2;
            m_viewport.y = 0;
        }
        else
        {
            m_viewport.height = static_cast<float>(width / m_aspectRatio);
            m_viewport.width = static_cast<float>(width);
            m_viewport.x = 0;
            m_viewport.y = (height - m_viewport.height) / 2;
        }
    }

    void StaticPicture::draw()
    {
        m_renderer->addDrawAction([this](VkCommandBuffer& cmdBuffer)
        {
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getHandle());
            vkCmdSetViewport(cmdBuffer, 0, 1, &m_viewport);
            m_descSetGroup.bind(cmdBuffer, m_pipeline->getPipelineLayout());

            unsigned int pushConst = 0;
            vkCmdPushConstants(cmdBuffer, m_pipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(unsigned int), &pushConst);

            m_renderer->drawFullScreenQuad(cmdBuffer);
        }, VulkanRenderer::DefaultRenderPassId);
    }
}