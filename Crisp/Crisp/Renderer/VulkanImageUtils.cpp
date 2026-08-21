#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/VulkanStagingBuffer.hpp>

namespace crisp {
namespace {
VkImageSubresourceRange createSubresourceRange(
    const VulkanImage& image,
    const uint32_t baseLayer,
    const uint32_t layerCount,
    const uint32_t baseMipLevel = 0,
    const uint32_t levelCount = 1) {
    return {
        .aspectMask = image.getAspectMask(),
        .baseMipLevel = baseMipLevel,
        .levelCount = levelCount,
        .baseArrayLayer = baseLayer,
        .layerCount = layerCount,
    };
}

VkBufferImageCopy createBufferImageCopy(
    const VulkanImage& image,
    const VkExtent3D& extent,
    const uint32_t baseLayer,
    const uint32_t layerCount,
    const uint32_t mipLevel = 0) {
    return {
        .bufferRowLength = extent.width,
        .bufferImageHeight = extent.height,
        .imageSubresource =
            {
                .aspectMask = image.getAspectMask(),
                .mipLevel = mipLevel,
                .baseArrayLayer = baseLayer,
                .layerCount = layerCount,
            },
        .imageExtent = extent,
    };
}
} // namespace

void fillImageLayer(VulkanImage& image, Renderer& renderer, const void* data, VkDeviceSize size, uint32_t layerIdx) {
    fillImageLayers(image, renderer, data, size, layerIdx, 1);
}

void fillImageLayers(
    VulkanImage& image, Renderer& renderer, const void* data, VkDeviceSize size, uint32_t layerIdx, uint32_t numLayers) {
    auto& device = renderer.getDevice();
    const auto staging = createStagingBuffer(device, data, size);
    submitAndWait(device.getGeneralQueue(), [&staging, &image, layerIdx, numLayers](const VulkanCommandEncoder& encoder) {
        const auto range = createSubresourceRange(image, layerIdx, numLayers, 0, image.getMipLevels());
        encoder.transitionLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kNullStage >> kTransferWrite, range);
        encoder.copyBufferToImage(*staging, image, createBufferImageCopy(image, image.getExtent(), layerIdx, numLayers));
        encoder.transitionLayout(
            image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentRead, range);
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

    const auto staging = createStagingBuffer(renderer.getDevice(), image.getData(), image.getByteSize());
    submitAndWait(
        renderer.getDevice().getGeneralQueue(), [&staging, &vulkanImage](const VulkanCommandEncoder& commandEncoder) {
            commandEncoder.transitionLayout(
                *vulkanImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                kNullStage >> kTransferWrite,
                vulkanImage->getFirstMipRange());
            commandEncoder.copyBufferToImage(
                *staging, *vulkanImage, createBufferImageCopy(*vulkanImage, vulkanImage->getExtent(), 0, 1));
            commandEncoder.generateMipmaps(*vulkanImage, kTransferWrite);
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

        // Stage all face data first, then submit one command buffer for this mip level.
        std::vector<std::unique_ptr<VulkanBuffer>> faceStaging;
        faceStaging.reserve(cubeMapMipLevel.size());
        for (const auto& face : cubeMapMipLevel) {
            faceStaging.push_back(createStagingBuffer(renderer.getDevice(), face.getData(), face.getByteSize()));
        }

        submitAndWait(
            renderer.getDevice().getGeneralQueue(),
            [&faceStaging, &vulkanImage, mipLevel, mipSize](const VulkanCommandEncoder& commandEncoder) {
                commandEncoder.transitionLayout(
                    *vulkanImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kNullStage >> kTransferWrite);
                for (uint32_t face = 0; face < faceStaging.size(); ++face) { // NOLINT
                    const VkExtent3D extent{mipSize, mipSize, 1};
                    commandEncoder.copyBufferToImage(
                        *faceStaging[face], *vulkanImage, createBufferImageCopy(*vulkanImage, extent, face, 1, mipLevel));
                }
                commandEncoder.transitionLayout(
                    *vulkanImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentRead);
            });
    }

    return vulkanImage;
}

std::unique_ptr<VulkanImage> createVulkanImage(
    Renderer& renderer, const VkDeviceSize size, const void* data, const VkImageCreateInfo imageCreateInfo) {
    auto image = std::make_unique<VulkanImage>(renderer.getDevice(), imageCreateInfo);

    const auto staging = createStagingBuffer(renderer.getDevice(), data, size);
    submitAndWait(
        renderer.getDevice().getGeneralQueue(), [&staging, img = image.get()](const VulkanCommandEncoder& encoder) {
            const auto fullRange = createSubresourceRange(*img, 0, 1, 0, img->getMipLevels());
            encoder.transitionLayout(
                *img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kNullStage >> kTransferWrite, fullRange);
            encoder.copyBufferToImage(*staging, *img, createBufferImageCopy(*img, img->getExtent(), 0, 1));
            encoder.generateMipmaps(*img);

            VkImageSubresourceRange mipRange = {};
            mipRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipRange.baseMipLevel = 0;
            mipRange.levelCount = img->getMipLevels();
            mipRange.baseArrayLayer = 0;
            mipRange.layerCount = 1;
            encoder.transitionLayout(
                *img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentRead, mipRange);
        });

    return image;
}

void updateCubeMap(
    VulkanImage& image, Renderer& renderer, const std::vector<Image>& cubeMapFaces, const uint32_t mipLevel) {
    CRISP_CHECK_EQ(cubeMapFaces.size(), 6);
    const uint32_t mipSize{cubeMapFaces.front().getWidth()};

    std::vector<std::unique_ptr<VulkanBuffer>> faceStaging;
    faceStaging.reserve(cubeMapFaces.size());
    for (const auto& face : cubeMapFaces) {
        faceStaging.push_back(createStagingBuffer(renderer.getDevice(), face.getData(), face.getByteSize()));
    }

    submitAndWait(
        renderer.getDevice().getGeneralQueue(),
        [&faceStaging, &image, mipLevel, mipSize](const VulkanCommandEncoder& encoder) {
            for (uint32_t i = 0; i < faceStaging.size(); ++i) {
                const VkExtent3D extent{mipSize, mipSize, 1};
                const auto range = createSubresourceRange(image, i, 1, mipLevel);
                encoder.transitionLayout(
                    image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kFragmentRead >> kTransferWrite, range);
                encoder.copyBufferToImage(*faceStaging[i], image, createBufferImageCopy(image, extent, i, 1, mipLevel));
                encoder.transitionLayout(
                    image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentRead, range);
            }
        });
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
