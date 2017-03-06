#include "Picture.hpp"

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
    Picture::Picture(uint32_t width, uint32_t height, VkFormat format, VulkanRenderer& renderer)
        : m_renderer(&renderer)
        , m_device(&renderer.getDevice())
        , m_extent({ width, height })
        , m_numChannels(4)
        
    {
        m_viewport.minDepth = 0.0f;
        m_viewport.maxDepth = 1.0f;
        recalculateViewport(m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height);
        m_pipeline = std::make_unique<FullScreenQuadPipeline>(m_renderer, &m_renderer->getDefaultRenderPass(), true);

        // create texture image
        auto byteSize = m_extent.width * m_extent.height * m_numChannels * sizeof(float);

        m_stagingTexBuffer = m_device->createStagingBuffer(byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

        std::vector<float> data(m_extent.width * m_extent.height * m_numChannels, 0.01f);
        m_device->fillStagingBuffer(m_stagingTexBuffer, data.data(), byteSize);

        m_imageArray = m_device->createDeviceImageArray(m_extent.width, m_extent.height, VulkanRenderer::NumVirtualFrames, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        m_device->updateDeviceImage(m_imageArray, m_stagingTexBuffer, byteSize, { m_extent.width, m_extent.height, 1u }, VulkanRenderer::NumVirtualFrames);
        m_device->transitionImageLayout(m_imageArray, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VulkanRenderer::NumVirtualFrames);

        // create view
        m_imageArrayView = m_device->createImageView(m_imageArray, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 0, VulkanRenderer::NumVirtualFrames);
        m_updatedImageIndex = 0;
       
        // create sampler
        m_sampler = m_device->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        // create vertex buffer
        std::vector<glm::vec2> vertices =
        {
            { -1.0f, -1.0f },
            { +1.0f, -1.0f },
            { +1.0f, +1.0f },
            { -1.0f, +1.0f }
        };
        m_vertexBuffer = m_device->createDeviceBuffer(sizeof(glm::vec2) * vertices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        m_device->fillDeviceBuffer(m_vertexBuffer, vertices.data(), sizeof(glm::vec2) * vertices.size());

        // create index buffer
        std::vector<glm::u16vec3> faces =
        {
            { 0, 1, 2 },
            { 0, 2, 3 }
        };
        m_indexBuffer = m_device->createDeviceBuffer(sizeof(glm::u16vec3) * faces.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        m_device->fillDeviceBuffer(m_indexBuffer, faces.data(), sizeof(glm::u16vec3) * faces.size());

        // descriptor set
        m_descriptorSet = m_pipeline->allocateDescriptorSet(0);

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = m_imageArrayView;
        imageInfo.sampler     = m_sampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = m_descriptorSet;
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo      = &imageInfo;

        descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet          = m_descriptorSet;
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(m_renderer->getDevice().getHandle(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        m_drawItem.pipeline            = m_pipeline->getHandle();
        m_drawItem.pipelineLayout      = m_pipeline->getPipelineLayout();
        m_drawItem.descriptorSetOffset = 0;
        m_drawItem.descriptorSets.push_back(m_descriptorSet);

        m_drawItem.vertexBufferBindingOffset = 0;
        m_drawItem.vertexBuffers.push_back(m_vertexBuffer);
        m_drawItem.vertexBufferOffsets.push_back(0);

        m_drawItem.indexBuffer       = m_indexBuffer;
        m_drawItem.indexType         = VK_INDEX_TYPE_UINT16;
        m_drawItem.indexBufferOffset = 0;

        m_drawItem.indexCount    = 6;
        m_drawItem.instanceCount = 1;
        m_drawItem.firstIndex    = 0;
        m_drawItem.vertexOffset  = 0;
        m_drawItem.firstInstance = 0;
    }

    Picture::~Picture()
    {
        vkDestroySampler(m_device->getHandle(), m_sampler, nullptr);
        vkDestroyImageView(m_device->getHandle(), m_imageArrayView, nullptr);

        m_device->destroyDeviceImage(m_imageArray);
        m_device->destroyStagingBuffer(m_stagingTexBuffer);

        m_device->destroyDeviceBuffer(m_vertexBuffer);
        m_device->destroyDeviceBuffer(m_indexBuffer);
    }

    void Picture::postTextureUpdate(vesper::RayTracerUpdate update)
    {
        // Add an update that stretches over three frames
        m_textureUpdates.emplace_back(std::make_pair(VulkanRenderer::NumVirtualFrames, update));
        
        uint32_t rowSize = update.width * m_numChannels * sizeof(float);
        for (int i = 0; i < update.height; i++)
        {
            uint32_t localflipY = update.height - 1 - i;
            uint32_t dstOffset = (m_extent.width * (update.y + localflipY) + update.x) * m_numChannels * sizeof(float);
            uint32_t srcIndex = i * update.width * m_numChannels;

            m_renderer->getDevice().updateStagingBuffer(m_stagingTexBuffer, &update.data[srcIndex], dstOffset, rowSize);
        }
    }

    void Picture::draw()
    {
        if (!m_textureUpdates.empty())
        {
            m_renderer->addCopyAction([this](VkCommandBuffer& cmdBuffer)
            {
                m_updatedImageIndex = (m_updatedImageIndex + 1) % VulkanRenderer::NumVirtualFrames;

                // Transition the destination image layer into transfer destination
                VkImageMemoryBarrier toCopyBarrierDst = {};
                toCopyBarrierDst.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                toCopyBarrierDst.oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                toCopyBarrierDst.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                toCopyBarrierDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toCopyBarrierDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toCopyBarrierDst.srcAccessMask       = VK_ACCESS_SHADER_READ_BIT;
                toCopyBarrierDst.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
                toCopyBarrierDst.image               = m_imageArray;
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
                vkCmdCopyBufferToImage(cmdBuffer, m_stagingTexBuffer, m_imageArray, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

                m_textureUpdates.erase(std::remove_if(m_textureUpdates.begin(), m_textureUpdates.end(), [](const std::pair<unsigned int, vesper::RayTracerUpdate>& item)
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
                readBarrierDst.image               = m_imageArray;
                readBarrierDst.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                readBarrierDst.subresourceRange.baseMipLevel   = 0;
                readBarrierDst.subresourceRange.levelCount     = 1;
                readBarrierDst.subresourceRange.baseArrayLayer = m_updatedImageIndex;
                readBarrierDst.subresourceRange.layerCount     = 1;
                vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &readBarrierDst);
            });
        }

        m_renderer->addDrawAction([this](VkCommandBuffer& cmdBuffer)
        {
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getHandle());
            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getPipelineLayout(),
                0, 1, &m_descriptorSet, 0, nullptr);

            vkCmdSetViewport(cmdBuffer, 0, 1, &m_viewport);

            vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBuffer, m_drawItem.vertexBufferOffsets.data());
            vkCmdBindIndexBuffer(cmdBuffer, m_drawItem.indexBuffer, m_drawItem.indexBufferOffset, m_drawItem.indexType);

            unsigned int pushConst = m_updatedImageIndex;
            vkCmdPushConstants(cmdBuffer, m_pipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(unsigned int), &pushConst);
            vkCmdDrawIndexed(cmdBuffer, 6, 1, 0, 0, 0);
        }, VulkanRenderer::DefaultRenderPassId);
    }

    void Picture::resize(int width, int height)
    {
        m_pipeline->resize(width, height);
        m_drawItem.pipeline       = m_pipeline->getHandle();
        m_drawItem.pipelineLayout = m_pipeline->getPipelineLayout();
        recalculateViewport(width, height);
    }

    void Picture::recalculateViewport(int screenWidth, int screenHeight)
    {
        m_viewport.x      = (screenWidth - static_cast<int>(m_extent.width)) / 2.0f;
        m_viewport.y      = (screenHeight - static_cast<int>(m_extent.height)) / 2.0f;
        m_viewport.width  = static_cast<float>(m_extent.width);
        m_viewport.height = static_cast<float>(m_extent.height);
    }
}