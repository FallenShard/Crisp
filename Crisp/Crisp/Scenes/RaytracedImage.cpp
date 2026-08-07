#include <Crisp/Scenes/RaytracedImage.hpp>

#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>

#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/Renderer.hpp>

namespace crisp {
RayTracedImage::RayTracedImage(uint32_t width, uint32_t height, Renderer* renderer)
    : m_extent({width, height, 1u})
    , m_channelCount(4)
    , m_viewport{} {
    m_viewport.minDepth = 0.0f;
    m_viewport.maxDepth = 1.0f;
    resize(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);

    // create texture image
    std::vector<float> data(m_extent.width * m_extent.height * m_channelCount, 0.01f);
    auto byteSize = data.size() * sizeof(float);

    m_stagingBuffer = std::make_unique<VulkanBuffer>(
        renderer->getDevice(), byteSize, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT, BufferMemoryType::HostUpload);
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
        renderer->enqueueResourceUpdate([this, i, stagingBuffer = m_stagingBuffer.get()](VkCommandBuffer cmdBuffer) {
            const VulkanCommandEncoder encoder(cmdBuffer);
            const VkImageSubresourceRange range{
                .aspectMask = m_image->getAspectMask(),
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = i,
                .layerCount = 1,
            };
            encoder.transitionLayout(
                *m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kNullStage >> kTransferWrite, range);
            const VkBufferImageCopy region{
                .bufferRowLength = m_extent.width,
                .bufferImageHeight = m_extent.height,
                .imageSubresource =
                    {
                        .aspectMask = m_image->getAspectMask(),
                        .mipLevel = 0,
                        .baseArrayLayer = i,
                        .layerCount = 1,
                    },
                .imageExtent = m_extent,
            };
            encoder.copyBufferToImage(*stagingBuffer, *m_image, region);
            encoder.transitionLayout(
                *m_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentRead, range);
        });
        m_imageViews.push_back(createView(renderer->getDevice(), *m_image, VK_IMAGE_VIEW_TYPE_2D, i, 1));
    }
    renderer->flushResourceUpdates(true);

    // create sampler
    m_sampler = createLinearClampSampler(renderer->getDevice());

    m_pipeline = renderer->createPipeline("Tonemapping.lua", renderer->getDefaultRasterizationPassDescriptor());
    m_materials.reserve(kRendererVirtualFrameCount);
    for (uint32_t i = 0; i < kRendererVirtualFrameCount; ++i) {
        auto& material = m_materials.emplace_back(std::make_unique<Material>(m_pipeline.get()));
        material->writeDescriptor(0, 0, *m_imageViews[i], *m_sampler);
    }
    renderer->getDevice().flushDescriptorUpdates();
}

void RayTracedImage::postTextureUpdate(RayTracerUpdate update) {
    // Add an update that stretches over three frames
    m_textureUpdates.emplace_back(kRendererVirtualFrameCount, update);

    uint32_t rowSize = update.width * m_channelCount * sizeof(float);
    for (int i = 0; i < update.height; i++) {
        uint32_t localflipY = update.height - 1 - i;
        uint32_t dstOffset = (m_extent.width * (update.y + localflipY) + update.x) * m_channelCount * sizeof(float);
        uint32_t srcIndex = i * update.width * m_channelCount;

        m_stagingBuffer->updateFromHost(&update.data[srcIndex], rowSize, dstOffset);
    }
}

void RayTracedImage::draw(Renderer* renderer) {
    if (!m_textureUpdates.empty()) {
        renderer->enqueueResourceUpdate([this, renderer](VkCommandBuffer cmdBuffer) {
            uint32_t frameIdx = renderer->getCurrentVirtualFrameIndex();
            const VulkanCommandEncoder encoder(cmdBuffer);
            const VkImageSubresourceRange range{
                .aspectMask = m_image->getAspectMask(),
                .baseMipLevel = 0,
                .levelCount = m_image->getMipLevels(),
                .baseArrayLayer = frameIdx,
                .layerCount = 1,
            };

            encoder.transitionLayout(
                *m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kFragmentRead >> kTransferWrite, range);

            // Perform the copy from the buffer that has accumulated the updates through memcpy
            std::vector<VkBufferImageCopy> copyRegions;
            copyRegions.resize(m_textureUpdates.size());

            size_t i = 0;
            for (auto& texUpdateItem : m_textureUpdates) {
                auto& texUpdate = texUpdateItem.second;
                copyRegions[i].bufferOffset =
                    (m_extent.width * texUpdate.y + texUpdate.x) * m_channelCount * sizeof(float);
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
            encoder.copyBufferToImage(m_stagingBuffer->getHandle(), *m_image, copyRegions);

            std::erase_if(m_textureUpdates, [](const auto& item) { return item.first == 0; });

            encoder.transitionLayout(
                *m_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentRead, range);
        });
    }

    renderer->enqueueDefaultPassDrawCommand([this, renderer](VkCommandBuffer cmdBuffer) {
        const VulkanCommandEncoder encoder(cmdBuffer);
        encoder.bindPipeline(*m_pipeline);
        encoder.bindDescriptorSets(m_materials[renderer->getCurrentVirtualFrameIndex()]->getDescriptorSetBinding());
        encoder.setViewport(m_viewport);

        renderer->drawFullScreenQuad(encoder);
    });
}

void RayTracedImage::resize(const uint32_t width, const uint32_t height) {
    m_viewport.x = (static_cast<float>(width) - static_cast<float>(m_extent.width)) / 2.0f;
    m_viewport.y = (static_cast<float>(height) - static_cast<float>(m_extent.height)) / 2.0f;
    m_viewport.width = static_cast<float>(m_extent.width);
    m_viewport.height = static_cast<float>(m_extent.height);
}
} // namespace crisp
