#include "RayTracedImage.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Vulkan/VulkanFormatTraits.hpp"
#include "Vulkan/VulkanBuffer.hpp"
#include "Vulkan/VulkanImage.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "Vulkan/VulkanSampler.hpp"

#include "Renderer/Texture.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Renderer/Material.hpp"

namespace crisp
{
    RayTracedImage::RayTracedImage(uint32_t width, uint32_t height, VkFormat format, Renderer* renderer)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
        , m_extent({ width, height, 1u })
        , m_numChannels(getNumChannels(format))
    {
        m_viewport.minDepth = 0.0f;
        m_viewport.maxDepth = 1.0f;
        resize(m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height);

        // create texture image
        auto byteSize = m_extent.width * m_extent.height * m_numChannels * sizeof(float);

        std::vector<float> data(m_extent.width * m_extent.height * m_numChannels, 0.01f);
        m_stagingBuffer = std::make_unique<VulkanBuffer>(m_device, byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        m_stagingBuffer->updateFromHost(data.data(), byteSize);

        m_texture = std::make_unique<Texture>(m_renderer, m_extent, Renderer::NumVirtualFrames, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_texture->fill(*m_stagingBuffer, byteSize, 0, 1);
        m_texture->fill(*m_stagingBuffer, byteSize, 1, 1);
        m_texture->fill(*m_stagingBuffer, byteSize, 2, 1);

        // create view
        m_textureView = m_texture->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, Renderer::NumVirtualFrames);
        m_updatedImageIndex = 0;

        // create sampler
        m_sampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_pipeline = createTonemappingPipeline(m_renderer, m_renderer->getDefaultRenderPass(), 0, true);
        m_material = std::make_unique<Material>(m_pipeline.get(), std::vector<uint32_t>{ 0 });
        m_device->postDescriptorWrite(m_material->makeDescriptorWrite(0, 0), m_textureView->getDescriptorInfo(m_sampler->getHandle()));
        m_device->flushDescriptorUpdates();
    }

    RayTracedImage::~RayTracedImage()
    {
    }

    void RayTracedImage::postTextureUpdate(RayTracerUpdate update)
    {
        // Add an update that stretches over three frames
        m_textureUpdates.emplace_back(std::make_pair(Renderer::NumVirtualFrames, std::move(update)));

        uint32_t rowSize = update.width * m_numChannels * sizeof(float);
        for (int i = 0; i < update.height; i++)
        {
            uint32_t localflipY = update.height - 1 - i;
            uint32_t dstOffset = (m_extent.width * (update.y + localflipY) + update.x) * m_numChannels * sizeof(float);
            uint32_t srcIndex = i * update.width * m_numChannels;

            m_stagingBuffer->updateFromHost(&update.data[srcIndex], rowSize, dstOffset);
        }
    }

    void RayTracedImage::draw()
    {
        if (!m_textureUpdates.empty())
        {
            m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
            {
                m_updatedImageIndex = (m_updatedImageIndex + 1) % Renderer::NumVirtualFrames;

                // Transition the destination image layer into transfer destination
                VkImageMemoryBarrier toCopyBarrierDst = {};
                toCopyBarrierDst.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                toCopyBarrierDst.oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                toCopyBarrierDst.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                toCopyBarrierDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toCopyBarrierDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toCopyBarrierDst.srcAccessMask       = VK_ACCESS_SHADER_READ_BIT;
                toCopyBarrierDst.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
                toCopyBarrierDst.image               = m_texture->getImage()->getHandle();
                toCopyBarrierDst.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                toCopyBarrierDst.subresourceRange.baseMipLevel   = 0;
                toCopyBarrierDst.subresourceRange.levelCount     = 1;
                toCopyBarrierDst.subresourceRange.baseArrayLayer = m_updatedImageIndex;
                toCopyBarrierDst.subresourceRange.layerCount     = 1;
                vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toCopyBarrierDst);

                // Perform the copy from the buffer that has accumulated the updates through memcpy
                std::vector<VkBufferImageCopy> copyRegions;
                copyRegions.resize(m_textureUpdates.size());

                size_t i = 0;
                for (auto& texUpdateItem : m_textureUpdates)
                {
                    auto& texUpdate = texUpdateItem.second;
                    copyRegions[i].bufferOffset       = (m_extent.width * texUpdate.y + texUpdate.x) * m_numChannels * sizeof(float);
                    copyRegions[i].bufferRowLength    = m_extent.width;
                    copyRegions[i].bufferImageHeight  = texUpdate.height;
                    copyRegions[i].imageExtent.width  = texUpdate.width;
                    copyRegions[i].imageExtent.height = texUpdate.height;
                    copyRegions[i].imageExtent.depth  = 1;
                    copyRegions[i].imageOffset        = { texUpdate.x, static_cast<int32_t>(m_extent.height) - texUpdate.y - texUpdate.height, 0 };
                    copyRegions[i].imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                    copyRegions[i].imageSubresource.baseArrayLayer = m_updatedImageIndex;
                    copyRegions[i].imageSubresource.layerCount     = 1;

                    texUpdateItem.first--;
                    i++;
                }
                vkCmdCopyBufferToImage(cmdBuffer, m_stagingBuffer->getHandle(), m_texture->getImage()->getHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

                m_textureUpdates.erase(std::remove_if(m_textureUpdates.begin(), m_textureUpdates.end(), [](const std::pair<unsigned int, RayTracerUpdate>& item)
                {
                    return item.first <= 0; // Erase those updates that have been written NumVirtualFrames times already
                }), m_textureUpdates.end());

                // Transition the destination image back into shader-readable resource
                VkImageMemoryBarrier readBarrierDst = {};
                readBarrierDst.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                readBarrierDst.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                readBarrierDst.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                readBarrierDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                readBarrierDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                readBarrierDst.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
                readBarrierDst.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
                readBarrierDst.image               = m_texture->getImage()->getHandle();
                readBarrierDst.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                readBarrierDst.subresourceRange.baseMipLevel   = 0;
                readBarrierDst.subresourceRange.levelCount     = 1;
                readBarrierDst.subresourceRange.baseArrayLayer = m_updatedImageIndex;
                readBarrierDst.subresourceRange.layerCount     = 1;
                vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &readBarrierDst);
            });
        }

        m_renderer->enqueueDefaultPassDrawCommand([this](VkCommandBuffer cmdBuffer)
        {
            m_pipeline->bind(cmdBuffer);
            m_material->bind(m_renderer->getCurrentVirtualFrameIndex(), cmdBuffer);
            vkCmdSetViewport(cmdBuffer, 0, 1, &m_viewport);

            m_pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, m_updatedImageIndex);
            m_renderer->drawFullScreenQuad(cmdBuffer);
        });
    }

    void RayTracedImage::resize(int width, int height)
    {
        m_viewport.x = (width - static_cast<int>(m_extent.width)) / 2.0f;
        m_viewport.y = (height - static_cast<int>(m_extent.height)) / 2.0f;
        m_viewport.width = static_cast<float>(m_extent.width);
        m_viewport.height = static_cast<float>(m_extent.height);
    }
}