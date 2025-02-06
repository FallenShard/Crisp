#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Renderer/VulkanBufferUtils.hpp>

namespace crisp {
void fillImageLayer(VulkanImage& image, Renderer& renderer, const void* data, VkDeviceSize size, uint32_t layerIdx) {
    fillImageLayers(image, renderer, data, size, layerIdx, 1);
}

void fillImageLayers(
    VulkanImage& image, Renderer& renderer, const void* data, VkDeviceSize size, uint32_t layerIdx, uint32_t numLayers) {
    auto stagingBuffer = createStagingBuffer(renderer.getDevice(), size, data);
    renderer.getDevice().getGeneralQueue().submitAndWait(
        [&stagingBuffer, &image, layerIdx, numLayers](VkCommandBuffer cmdBuffer) {
            image.transitionLayout(
                cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layerIdx, numLayers, kNullStage >> kTransferWrite);
            image.copyFrom(cmdBuffer, *stagingBuffer, layerIdx, numLayers);
            image.transitionLayout(
                cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, layerIdx, numLayers, kTransferWrite >> kFragmentRead);
        });
}

std::unique_ptr<VulkanImage> createVulkanImage(Renderer& renderer, const Image& image, const VkFormat format) {
    auto vulkanImage = std::make_unique<VulkanImage>(
        renderer.getDevice(),
        VulkanImageDescription{
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {image.getWidth(), image.getHeight(), 1u},
            .mipLevelCount = image.getMipLevels(),
            .layerCount = 1,
            .usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        });

    auto stagingBuffer = createStagingBuffer(renderer.getDevice(), image.getByteSize(), image.getData());
    renderer.getDevice().getGeneralQueue().submitAndWait([&stagingBuffer, &vulkanImage](VkCommandBuffer cmdBuffer) {
        VulkanCommandEncoder commandEncoder(cmdBuffer);
        commandEncoder.transitionLayout(
            *vulkanImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            kNullStage >> kTransferWrite,
            vulkanImage->getFirstMipRange());
        vulkanImage->copyFrom(cmdBuffer, *stagingBuffer, 0, 1);
        vulkanImage->buildMipmaps(cmdBuffer, kTransferWrite);
        commandEncoder.transitionLayout(
            *vulkanImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentRead);
    });

    return vulkanImage;
}

std::unique_ptr<VulkanImage> createVulkanCubeMap(
    Renderer& renderer, const std::span<const std::vector<Image>> cubeMapFaceMips, const VkFormat format) {
    CRISP_CHECK(!cubeMapFaceMips.empty());
    const uint32_t cubeMapSize{cubeMapFaceMips.front().front().getWidth()};
    auto vulkanImage = std::make_unique<VulkanImage>(
        renderer.getDevice(),
        VulkanImageDescription{
            .format = format,
            .extent = {cubeMapSize, cubeMapSize, 1u},
            .mipLevelCount = static_cast<uint32_t>(cubeMapFaceMips.size()),
            .layerCount = 6,
            .usageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .createFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        });

    for (uint32_t mipLevel = 0; mipLevel < cubeMapFaceMips.size(); ++mipLevel) { // NOLINT
        const auto& cubeMapMipLevel{cubeMapFaceMips[mipLevel]};
        const uint32_t mipSize{cubeMapMipLevel.front().getWidth()};
        renderer.getDevice().getGeneralQueue().submitAndWait([&vulkanImage](const VkCommandBuffer cmdBuffer) {
            VulkanCommandEncoder commandEncoder(cmdBuffer);
            commandEncoder.transitionLayout(
                *vulkanImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kNullStage >> kTransferWrite);
        });
        for (uint32_t face = 0; face < cubeMapMipLevel.size(); ++face) { // NOLINT
            auto stagingBuffer = createStagingBuffer(
                renderer.getDevice(), cubeMapMipLevel[face].getByteSize(), cubeMapMipLevel[face].getData());
            renderer.getDevice().getGeneralQueue().submitAndWait(
                [mipSize, &vulkanImage, &stagingBuffer, face, mipLevel](const VkCommandBuffer cmdBuffer) {
                    vulkanImage->copyFrom(cmdBuffer, *stagingBuffer, VkExtent3D{mipSize, mipSize, 1}, face, 1, mipLevel);
                });
        }
        renderer.getDevice().getGeneralQueue().submitAndWait([&vulkanImage](const VkCommandBuffer cmdBuffer) {
            VulkanCommandEncoder commandEncoder(cmdBuffer);
            commandEncoder.transitionLayout(
                *vulkanImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentRead);
        });
    }

    return vulkanImage;
}

std::unique_ptr<VulkanImage> createVulkanImage(
    Renderer& renderer, const VkDeviceSize size, const void* data, const VkImageCreateInfo imageCreateInfo) {
    auto image = std::make_unique<VulkanImage>(renderer.getDevice(), imageCreateInfo);

    std::shared_ptr<VulkanBuffer> stagingBuffer = createStagingBuffer(renderer.getDevice(), size, data);
    renderer.enqueueResourceUpdate([stagingBuffer, image = image.get()](VkCommandBuffer cmdBuffer) {
        image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1, kNullStage >> kTransferWrite);
        image->copyFrom(cmdBuffer, *stagingBuffer, 0, 1);
        image->buildMipmaps(cmdBuffer);

        VkImageSubresourceRange mipRange = {};
        mipRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        mipRange.baseMipLevel = 0;
        mipRange.levelCount = image->getMipLevels();
        mipRange.baseArrayLayer = 0;
        mipRange.layerCount = 1;
        image->transitionLayout(
            cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipRange, kTransferWrite >> kFragmentRead);
    });

    return image;
}

void updateCubeMap(
    VulkanImage& image, Renderer& renderer, const std::vector<Image>& cubeMapFaces, const uint32_t mipLevel) {
    CRISP_CHECK_EQ(cubeMapFaces.size(), 6);
    const uint32_t mipSize{cubeMapFaces.front().getWidth()};
    for (uint32_t i = 0; i < cubeMapFaces.size(); ++i) {
        std::shared_ptr<VulkanBuffer> stagingBuffer =
            createStagingBuffer(renderer.getDevice(), cubeMapFaces[i].getByteSize(), cubeMapFaces[i].getData());
        renderer.enqueueResourceUpdate([&image, layer = i, mipLevel, mipSize, stagingBuffer](VkCommandBuffer cmdBuffer) {
            const VkExtent3D extent{mipSize, mipSize, 1};
            image.transitionLayout(
                cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layer, 1, mipLevel, 1, kFragmentRead >> kTransferWrite);
            image.copyFrom(cmdBuffer, *stagingBuffer, extent, layer, 1, mipLevel);
            image.transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                layer,
                1,
                mipLevel,
                1,
                kTransferWrite >> kFragmentRead);
        });
    }
}

std::unique_ptr<VulkanImage> createMipmapCubeMap(
    Renderer* renderer, const uint32_t width, const uint32_t height, const uint32_t mipLevels) {
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
    const Renderer& renderer, const VkFormat format, const VkExtent3D extent) {
    VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = format;
    createInfo.extent = extent;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return std::make_unique<VulkanImage>(renderer.getDevice(), createInfo);
}

std::unique_ptr<VulkanImage> createStorageImage(
    VulkanDevice& device, const uint32_t layerCount, const uint32_t width, const uint32_t height, const VkFormat format) {
    VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = format;
    createInfo.extent = {width, height, 1u};
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = layerCount;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return std::make_unique<VulkanImage>(device, createInfo);
}

} // namespace crisp