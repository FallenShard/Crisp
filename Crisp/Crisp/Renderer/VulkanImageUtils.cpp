#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Renderer/VulkanBufferUtils.hpp>

namespace crisp
{
    void fillLayer(VulkanImage& image, Renderer* renderer, const void* data, VkDeviceSize size, uint32_t layerIdx)
    {
        fillLayers(image, renderer, data, size, layerIdx, 1);
    }

    void fillLayers(VulkanImage& image, Renderer* renderer, const void* data, VkDeviceSize size, uint32_t layerIdx, uint32_t numLayers)
    {
        std::shared_ptr<VulkanBuffer> stagingBuffer = createStagingBuffer(renderer->getDevice(), size, data);

        renderer->enqueueResourceUpdate([&image, layerIdx, numLayers, renderer, stagingBuffer](VkCommandBuffer cmdBuffer)
        {
            image.transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layerIdx, numLayers, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            image.copyFrom(cmdBuffer, *stagingBuffer, layerIdx, numLayers);
            image.transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, layerIdx, numLayers, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
    }

    std::unique_ptr<VulkanImage> createTexture(Renderer* renderer, const Image& image, VkFormat format)
    {
        VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageInfo.flags         = 0;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = format;
        imageInfo.extent        = { image.getWidth(), image.getHeight(), 1u };
        imageInfo.mipLevels     = image.getMipLevels();
        imageInfo.arrayLayers   = 1;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        auto vulkanImage = std::make_unique<VulkanImage>(renderer->getDevice(), imageInfo, VK_IMAGE_ASPECT_COLOR_BIT);

        std::shared_ptr<VulkanBuffer> stagingBuffer = createStagingBuffer(renderer->getDevice(), image.getByteSize(), image.getData());
        renderer->enqueueResourceUpdate([renderer, stagingBuffer, image = vulkanImage.get()](VkCommandBuffer cmdBuffer)
        {
            image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            image->copyFrom(cmdBuffer, *stagingBuffer, 0, 1);
            image->buildMipmaps(cmdBuffer);

            VkImageSubresourceRange mipRange = {};
            mipRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            mipRange.baseMipLevel   = 0;
            mipRange.levelCount     = image->getMipLevels();
            mipRange.baseArrayLayer = 0;
            mipRange.layerCount     = 1;
            image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

        return vulkanImage;
    }

    std::unique_ptr<VulkanImage> createTexture(Renderer* renderer, const std::string& filename, const VkFormat format, const FlipOnLoad flipOnLoad)
    {
        return createTexture(renderer, loadImage(renderer->getResourcesPath() / "Textures" / filename, 4, flipOnLoad).unwrap(), format);
    }

    std::unique_ptr<VulkanImage> createTexture(Renderer* renderer, VkDeviceSize size, const void* data, VkImageCreateInfo imageCreateInfo, VkImageAspectFlags imageAspect)
    {
        auto image = std::make_unique<VulkanImage>(renderer->getDevice(), imageCreateInfo, imageAspect);

        std::shared_ptr<VulkanBuffer> stagingBuffer = createStagingBuffer(renderer->getDevice(), size, data);
        renderer->enqueueResourceUpdate([renderer, stagingBuffer, image = image.get()](VkCommandBuffer cmdBuffer)
        {
            image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            image->copyFrom(cmdBuffer, *stagingBuffer, 0, 1);
            image->buildMipmaps(cmdBuffer);

            VkImageSubresourceRange mipRange = {};
            mipRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            mipRange.baseMipLevel   = 0;
            mipRange.levelCount     = image->getMipLevels();
            mipRange.baseArrayLayer = 0;
            mipRange.layerCount     = 1;
            image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

        return image;
    }

    std::unique_ptr<VulkanImage> createEnvironmentMap(Renderer* renderer, const std::string& filename, const VkFormat format, const FlipOnLoad flipOnLoad)
    {
        return createTexture(renderer, loadImage(renderer->getResourcesPath() / "Textures/EnvironmentMaps" / filename, 4, flipOnLoad).unwrap(), format);
    }

    std::unique_ptr<VulkanImage> createMipmapCubeMap(Renderer* renderer, const uint32_t width, const uint32_t height, const uint32_t mipLevels)
    {
        VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = VK_FORMAT_R16G16B16A16_SFLOAT;
        imageInfo.extent        = { width, height, 1 };
        imageInfo.mipLevels     = mipLevels;
        imageInfo.arrayLayers   = 6;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        return std::make_unique<VulkanImage>(renderer->getDevice(), imageInfo, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}