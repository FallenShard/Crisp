#include <Crisp/Scenes/RaytracedImage.hpp>

#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/Renderer.hpp>

namespace crisp {
RayTracedImage::RayTracedImage(uint32_t width, uint32_t height, Renderer* renderer)
    : m_extent({width, height, 1u})
    , m_numChannels(4) {
    m_viewport.minDepth = 0.0f;
    m_viewport.maxDepth = 1.0f;
    resize(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);

    // create texture image
    std::vector<float> data(m_extent.width * m_extent.height * m_numChannels, 0.01f);
    auto byteSize = data.size() * sizeof(float);

    m_stagingBuffer = std::make_unique<StagingVulkanBuffer>(renderer->getDevice(), byteSize);
    m_stagingBuffer->updateFromHost(data.data(), byteSize, 0);

    m_image = std::make_unique<VulkanImage>(
        renderer->getDevice(),
        m_extent,
        kRendererVirtualFrameCount,
        1,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        0);

    for (uint32_t i = 0; i < kRendererVirtualFrameCount; ++i) {
        renderer->enqueueResourceUpdate(
            [this, i, size = byteSize, stagingBuffer = m_stagingBuffer.get()](VkCommandBuffer cmdBuffer) {
                m_image->transitionLayout(
                    cmdBuffer,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    i,
                    1,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);
                m_image->copyFrom(cmdBuffer, *stagingBuffer, i, 1);
                m_image->transitionLayout(
                    cmdBuffer,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    i,
                    1,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            });
        m_imageViews.push_back(createView(renderer->getDevice(), *m_image, VK_IMAGE_VIEW_TYPE_2D, i, 1));
    }
    renderer->flushResourceUpdates(true);

    // create sampler
    m_sampler = createLinearClampSampler(renderer->getDevice());

    m_pipeline = renderer->createPipeline("Tonemapping.lua", renderer->getDefaultRenderPass(), 0);
    m_material = std::make_unique<Material>(m_pipeline.get());
    for (uint32_t i = 0; i < kRendererVirtualFrameCount; ++i) {
        m_material->writeDescriptor(0, 0, i, *m_imageViews[i], m_sampler.get());
    }
    renderer->getDevice().flushDescriptorUpdates();
}

void RayTracedImage::postTextureUpdate(RayTracerUpdate update) {
    // Add an update that stretches over three frames
    m_textureUpdates.emplace_back(kRendererVirtualFrameCount, update);

    uint32_t rowSize = update.width * m_numChannels * sizeof(float);
    for (int i = 0; i < update.height; i++) {
        uint32_t localflipY = update.height - 1 - i;
        uint32_t dstOffset = (m_extent.width * (update.y + localflipY) + update.x) * m_numChannels * sizeof(float);
        uint32_t srcIndex = i * update.width * m_numChannels;

        m_stagingBuffer->updateFromHost(&update.data[srcIndex], rowSize, dstOffset);
    }
}

void RayTracedImage::draw(Renderer* renderer) {
    if (!m_textureUpdates.empty()) {
        renderer->enqueueResourceUpdate([this, renderer](VkCommandBuffer cmdBuffer) {
            uint32_t frameIdx = renderer->getCurrentVirtualFrameIndex();

            m_image->transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                frameIdx,
                1,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);

            // Perform the copy from the buffer that has accumulated the updates through memcpy
            std::vector<VkBufferImageCopy> copyRegions;
            copyRegions.resize(m_textureUpdates.size());

            size_t i = 0;
            for (auto& texUpdateItem : m_textureUpdates) {
                auto& texUpdate = texUpdateItem.second;
                copyRegions[i].bufferOffset =
                    (m_extent.width * texUpdate.y + texUpdate.x) * m_numChannels * sizeof(float);
                copyRegions[i].bufferRowLength = m_extent.width;
                copyRegions[i].bufferImageHeight = texUpdate.height;
                copyRegions[i].imageExtent.width = texUpdate.width;
                copyRegions[i].imageExtent.height = texUpdate.height;
                copyRegions[i].imageExtent.depth = 1;
                copyRegions[i].imageOffset = {
                    texUpdate.x, static_cast<int32_t>(m_extent.height) - texUpdate.y - texUpdate.height, 0};
                copyRegions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegions[i].imageSubresource.baseArrayLayer = frameIdx;
                copyRegions[i].imageSubresource.layerCount = 1;

                texUpdateItem.first--;
                i++;
            }
            vkCmdCopyBufferToImage(
                cmdBuffer,
                m_stagingBuffer->getHandle(),
                m_image->getHandle(),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                static_cast<uint32_t>(copyRegions.size()),
                copyRegions.data());

            m_textureUpdates.erase(
                std::remove_if(
                    m_textureUpdates.begin(),
                    m_textureUpdates.end(),
                    [](const std::pair<unsigned int, RayTracerUpdate>& item) {
                        return item.first <= 0;
                        // Erase those updates that have been written NumVirtualFrames times
                        // already
                    }),
                m_textureUpdates.end());

            m_image->transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                frameIdx,
                1,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
    }

    renderer->enqueueDefaultPassDrawCommand([this, renderer](VkCommandBuffer cmdBuffer) {
        m_pipeline->bind(cmdBuffer);
        m_material->bind(renderer->getCurrentVirtualFrameIndex(), cmdBuffer);
        vkCmdSetViewport(cmdBuffer, 0, 1, &m_viewport);

        renderer->drawFullScreenQuad(cmdBuffer);
    });
}

void RayTracedImage::resize(int width, int height) {
    m_viewport.x = static_cast<float>(width - static_cast<int>(m_extent.width)) / 2.0f;
    m_viewport.y = static_cast<float>(height - static_cast<int>(m_extent.height)) / 2.0f;
    m_viewport.width = static_cast<float>(m_extent.width);
    m_viewport.height = static_cast<float>(m_extent.height);
}
} // namespace crisp