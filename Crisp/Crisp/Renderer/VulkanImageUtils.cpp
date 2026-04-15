#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Core/Checks.hpp>

namespace crisp {
void fillImageLayer(VulkanImage& image, Renderer& renderer, const void* data, VkDeviceSize size, uint32_t layerIdx) {
    fillImageLayers(image, renderer, data, size, layerIdx, 1);
}

void fillImageLayers(
    VulkanImage& image, Renderer& renderer, const void* data, VkDeviceSize size, uint32_t layerIdx, uint32_t numLayers) {
    auto& belt = renderer.getStagingBelt();
    renderer.getDevice().getGeneralQueue().submitAndWait(
        [&belt, &image, layerIdx, numLayers, data, size](VkCommandBuffer cmdBuffer) {
            image.transitionLayout(
                cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layerIdx, numLayers, kNullStage >> kTransferWrite);
            belt.uploadImage(cmdBuffer, image, layerIdx, numLayers, 0, data, size);
            image.transitionLayout(
                cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, layerIdx, numLayers, kTransferWrite >> kFragmentRead);
        });
    belt.reclaimAll();
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

    auto& belt = renderer.getStagingBelt();
    renderer.getDevice().getGeneralQueue().submitAndWait(
        [&belt, &vulkanImage, &image](VkCommandBuffer cmdBuffer) {
            VulkanCommandEncoder commandEncoder(cmdBuffer);
            commandEncoder.transitionLayout(
                *vulkanImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                kNullStage >> kTransferWrite,
                vulkanImage->getFirstMipRange());
            belt.uploadImage(cmdBuffer, *vulkanImage, 0, 1, 0, image.getData(), image.getByteSize());
            vulkanImage->buildMipmaps(cmdBuffer, kTransferWrite);
            commandEncoder.transitionLayout(
                *vulkanImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentRead);
        });
    belt.reclaimAll();

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

    auto& belt = renderer.getStagingBelt();
    for (uint32_t mipLevel = 0; mipLevel < cubeMapFaceMips.size(); ++mipLevel) { // NOLINT
        const auto& cubeMapMipLevel{cubeMapFaceMips[mipLevel]};
        const uint32_t mipSize{cubeMapMipLevel.front().getWidth()};

        // Stage all face data first, then submit one command buffer for this mip level.
        renderer.getDevice().getGeneralQueue().submitAndWait(
            [&belt, &vulkanImage, &cubeMapMipLevel, mipLevel, mipSize](const VkCommandBuffer cmdBuffer) {
                VulkanCommandEncoder commandEncoder(cmdBuffer);
                commandEncoder.transitionLayout(
                    *vulkanImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kNullStage >> kTransferWrite);
                for (uint32_t face = 0; face < cubeMapMipLevel.size(); ++face) { // NOLINT
                    const VkExtent3D extent{mipSize, mipSize, 1};
                    belt.uploadImage(
                        cmdBuffer,
                        *vulkanImage,
                        extent,
                        face,
                        1,
                        mipLevel,
                        cubeMapMipLevel[face].getData(),
                        cubeMapMipLevel[face].getByteSize());
                }
                commandEncoder.transitionLayout(
                    *vulkanImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentRead);
            });
        belt.reclaimAll();
    }

    return vulkanImage;
}

std::unique_ptr<VulkanImage> createVulkanImage(
    Renderer& renderer, const VkDeviceSize size, const void* data, const VkImageCreateInfo imageCreateInfo) {
    auto image = std::make_unique<VulkanImage>(renderer.getDevice(), imageCreateInfo);

    auto& belt = renderer.getStagingBelt();
    renderer.getDevice().getGeneralQueue().submitAndWait(
        [&belt, img = image.get(), data, size](VkCommandBuffer cmdBuffer) {
            img->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1, kNullStage >> kTransferWrite);
            belt.uploadImage(cmdBuffer, *img, 0, 1, 0, data, size);
            img->buildMipmaps(cmdBuffer);

            VkImageSubresourceRange mipRange = {};
            mipRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipRange.baseMipLevel = 0;
            mipRange.levelCount = img->getMipLevels();
            mipRange.baseArrayLayer = 0;
            mipRange.layerCount = 1;
            img->transitionLayout(
                cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipRange, kTransferWrite >> kFragmentRead);
        });
    belt.reclaimAll();

    return image;
}

void updateCubeMap(
    VulkanImage& image, Renderer& renderer, const std::vector<Image>& cubeMapFaces, const uint32_t mipLevel) {
    CRISP_CHECK_EQ(cubeMapFaces.size(), 6);
    const uint32_t mipSize{cubeMapFaces.front().getWidth()};

    auto& belt = renderer.getStagingBelt();
    renderer.getDevice().getGeneralQueue().submitAndWait(
        [&belt, &image, &cubeMapFaces, mipLevel, mipSize](VkCommandBuffer cmdBuffer) {
            for (uint32_t i = 0; i < cubeMapFaces.size(); ++i) {
                const VkExtent3D extent{mipSize, mipSize, 1};
                image.transitionLayout(
                    cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, i, 1, mipLevel, 1, kFragmentRead >> kTransferWrite);
                belt.uploadImage(
                    cmdBuffer, image, extent, i, 1, mipLevel, cubeMapFaces[i].getData(), cubeMapFaces[i].getByteSize());
                image.transitionLayout(
                    cmdBuffer,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    i,
                    1,
                    mipLevel,
                    1,
                    kTransferWrite >> kFragmentRead);
            }
        });
    belt.reclaimAll();
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
