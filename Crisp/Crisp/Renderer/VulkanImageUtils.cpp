#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Common/Checks.hpp>
#include <Crisp/Renderer/VulkanBufferUtils.hpp>

namespace crisp
{
void fillImageLayer(VulkanImage& image, Renderer& renderer, const void* data, VkDeviceSize size, uint32_t layerIdx)
{
    fillImageLayers(image, renderer, data, size, layerIdx, 1);
}

void fillImageLayers(
    VulkanImage& image, Renderer& renderer, const void* data, VkDeviceSize size, uint32_t layerIdx, uint32_t numLayers)
{
    std::shared_ptr<VulkanBuffer> stagingBuffer = createStagingBuffer(renderer.getDevice(), size, data);
    renderer.enqueueResourceUpdate(
        [&image, layerIdx, numLayers, stagingBuffer](VkCommandBuffer cmdBuffer)
        {
            image.transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                layerIdx,
                numLayers,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
            image.copyFrom(cmdBuffer, *stagingBuffer, layerIdx, numLayers);
            image.transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                layerIdx,
                numLayers,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
}

std::unique_ptr<VulkanImage> createVulkanImage(Renderer& renderer, const Image& image, const VkFormat format)
{
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.flags = 0;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {image.getWidth(), image.getHeight(), 1u};
    imageInfo.mipLevels = image.getMipLevels();
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    auto vulkanImage = std::make_unique<VulkanImage>(renderer.getDevice(), imageInfo);

    std::shared_ptr<VulkanBuffer> stagingBuffer =
        createStagingBuffer(renderer.getDevice(), image.getByteSize(), image.getData());
    renderer.enqueueResourceUpdate(
        [stagingBuffer, image = vulkanImage.get()](VkCommandBuffer cmdBuffer)
        {
            image->transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0,
                1,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
            image->copyFrom(cmdBuffer, *stagingBuffer, 0, 1);
            image->buildMipmaps(cmdBuffer);

            VkImageSubresourceRange mipRange = {};
            mipRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipRange.baseMipLevel = 0;
            mipRange.levelCount = image->getMipLevels();
            mipRange.baseArrayLayer = 0;
            mipRange.layerCount = 1;
            image->transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                mipRange,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

    return vulkanImage;
}

std::unique_ptr<VulkanImage> createVulkanCubeMap(
    Renderer& renderer, const std::vector<std::vector<Image>>& cubeMapFaceMips, VkFormat format)
{
    CRISP_CHECK(cubeMapFaceMips.size() > 0);
    const uint32_t cubeMapSize{cubeMapFaceMips.front().front().getWidth()};
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {cubeMapSize, cubeMapSize, 1u};
    imageInfo.mipLevels = static_cast<uint32_t>(cubeMapFaceMips.size());
    imageInfo.arrayLayers = 6;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    auto vulkanImage = std::make_unique<VulkanImage>(renderer.getDevice(), imageInfo);

    for (uint32_t k = 0; k < cubeMapFaceMips.size(); ++k)
    {
        const auto& cubeMapMipLevel{cubeMapFaceMips[k]};
        const uint32_t mipSize{cubeMapMipLevel.front().getWidth()};
        for (uint32_t i = 0; i < cubeMapMipLevel.size(); ++i)
        {
            std::shared_ptr<VulkanBuffer> stagingBuffer = createStagingBuffer(
                renderer.getDevice(), cubeMapMipLevel[i].getByteSize(), cubeMapMipLevel[i].getData());
            renderer.enqueueResourceUpdate(
                [&image = *vulkanImage, layer = i, mipLevel = k, mipSize, stagingBuffer](VkCommandBuffer cmdBuffer)
                {
                    const VkExtent3D extent{mipSize, mipSize, 1};
                    image.transitionLayout(
                        cmdBuffer,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        layer,
                        1,
                        mipLevel,
                        1,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT);
                    image.copyFrom(cmdBuffer, *stagingBuffer, extent, layer, 1, mipLevel);
                    image.transitionLayout(
                        cmdBuffer,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        layer,
                        1,
                        mipLevel,
                        1,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                });
            // fillImageLayer(*vulkanImage, *renderer, cubeMapMipLevel[i].getData(), cubeMapMipLevel[i].getByteSize(),
            // i);
        }
    }

    return vulkanImage;
}

std::unique_ptr<VulkanImage> createVulkanImage(
    Renderer& renderer, const VkDeviceSize size, const void* data, const VkImageCreateInfo imageCreateInfo)
{
    auto image = std::make_unique<VulkanImage>(renderer.getDevice(), imageCreateInfo);

    std::shared_ptr<VulkanBuffer> stagingBuffer = createStagingBuffer(renderer.getDevice(), size, data);
    renderer.enqueueResourceUpdate(
        [stagingBuffer, image = image.get()](VkCommandBuffer cmdBuffer)
        {
            image->transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0,
                1,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
            image->copyFrom(cmdBuffer, *stagingBuffer, 0, 1);
            image->buildMipmaps(cmdBuffer);

            VkImageSubresourceRange mipRange = {};
            mipRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipRange.baseMipLevel = 0;
            mipRange.levelCount = image->getMipLevels();
            mipRange.baseArrayLayer = 0;
            mipRange.layerCount = 1;
            image->transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                mipRange,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

    return image;
}

void updateCubeMap(
    VulkanImage& image, Renderer& renderer, const std::vector<Image>& cubeMapFaces, const uint32_t mipLevel)
{
    CRISP_CHECK_EQ(cubeMapFaces.size(), 6);
    const uint32_t mipSize{cubeMapFaces.front().getWidth()};
    for (uint32_t i = 0; i < cubeMapFaces.size(); ++i)
    {
        std::shared_ptr<VulkanBuffer> stagingBuffer =
            createStagingBuffer(renderer.getDevice(), cubeMapFaces[i].getByteSize(), cubeMapFaces[i].getData());
        renderer.enqueueResourceUpdate(
            [&image, layer = i, mipLevel, mipSize, stagingBuffer](VkCommandBuffer cmdBuffer)
            {
                const VkExtent3D extent{mipSize, mipSize, 1};
                image.transitionLayout(
                    cmdBuffer,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    layer,
                    1,
                    mipLevel,
                    1,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);
                image.copyFrom(cmdBuffer, *stagingBuffer, extent, layer, 1, mipLevel);
                image.transitionLayout(
                    cmdBuffer,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    layer,
                    1,
                    mipLevel,
                    1,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            });
    }
}

std::unique_ptr<VulkanImage> createMipmapCubeMap(
    Renderer* renderer, const uint32_t width, const uint32_t height, const uint32_t mipLevels)
{
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 6;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return std::make_unique<VulkanImage>(renderer->getDevice(), imageInfo);
}

std::unique_ptr<VulkanImage> createSampledStorageImage(
    const Renderer& renderer, VkFormat format, const VkExtent3D extent)
{
    VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = format;
    createInfo.extent = extent;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = RendererConfig::VirtualFrameCount;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return std::make_unique<VulkanImage>(renderer.getDevice(), createInfo);
}

// void transitionComputeWriteToFragmentShading(VulkanImage& image, const VkImageSubresourceRange range) {
//     VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
//     barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
//     barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//     barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
//     barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
//     barrier.image = tex->getHandle();
//     barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     barrier.subresourceRange.baseMipLevel = 0;
//     barrier.subresourceRange.levelCount = 1;
//     barrier.subresourceRange.baseArrayLayer = frameIndex;
//     barrier.subresourceRange.layerCount = 1;
//
//     image.transitionLayout
//
//     vkCmdPipelineBarrier(
//         cmdBuffer.getHandle(),
//         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//         0,
//         0,
//         nullptr,
//         0,
//         nullptr,
//         0,
//         &barrier);
// }

} // namespace crisp