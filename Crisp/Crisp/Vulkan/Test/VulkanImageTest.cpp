#include <Crisp/Vulkan/Test/VulkanTest.hpp>

#include <Crisp/Vulkan/VulkanImage.hpp>

namespace crisp {
namespace {
using VulkanImageTest = VulkanTest;

using ::testing::ElementsAreArray;

TEST_F(VulkanImageTest, ChangingLayouts) {
    VkImageCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.arrayLayers = 1;
    createInfo.extent = {100, 100, 1};
    createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    createInfo.mipLevels = 1;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VulkanImage image(*device_, createInfo);
    EXPECT_THAT(image, HandleIsValid());
    EXPECT_EQ(image.getLayout(), VK_IMAGE_LAYOUT_UNDEFINED);

    {
        const ScopeCommandExecutor executor(*device_);
        const auto& cmdBuffer = executor.cmdBuffer.getHandle();
        image.transitionLayoutDirect(
            cmdBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            0,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT);
        EXPECT_EQ(image.getLayout(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        image.transitionLayoutDirect(
            cmdBuffer,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_WRITE_BIT);
        EXPECT_EQ(image.getLayout(), VK_IMAGE_LAYOUT_GENERAL);
    }
}

TEST_F(VulkanImageTest, CreateLayeredImageWithMipMaps) {
    VkImageCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.arrayLayers = 5;
    createInfo.extent = {100, 150, 1};
    createInfo.format = VK_FORMAT_R8G8_UNORM;
    createInfo.mipLevels = 3;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VulkanImage image(*device_, createInfo);
    EXPECT_THAT(image, HandleIsValid());
    EXPECT_THAT(image.getWidth(), 100);
    EXPECT_THAT(image.getHeight(), 150);
    EXPECT_THAT(image.getFullRange().aspectMask, VK_IMAGE_ASPECT_COLOR_BIT);
    EXPECT_THAT(image.getFullRange().layerCount, 5);
    EXPECT_THAT(image.getFullRange().levelCount, 3);
    EXPECT_THAT(image.getFormat(), VK_FORMAT_R8G8_UNORM);
    EXPECT_THAT(image.getLayout(), VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST_F(VulkanImageTest, FillImageRoundtrip) {
    VkImageCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.arrayLayers = 5;
    createInfo.extent = {100, 150, 1};
    createInfo.format = VK_FORMAT_R8G8_UNORM;
    createInfo.mipLevels = 1;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VulkanImage image(*device_, createInfo);
    EXPECT_THAT(image, HandleIsValid());

    std::vector<uint8_t> pixelData(2 * 100 * 150);
    for (uint32_t i = 0; i < 100; ++i) {
        for (uint32_t j = 0; j < 150; ++j) {
            pixelData[i * 150 * 2 + j * 2 + 0] = static_cast<uint8_t>(i);
            pixelData[i * 150 * 2 + j * 2 + 1] = static_cast<uint8_t>(i * 2);
        }
    }

    StagingVulkanBuffer stagingBuffer(*device_, pixelData.size());
    stagingBuffer.updateFromHost(pixelData);

    StagingVulkanBuffer downloadBuffer(*device_, pixelData.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    {
        const ScopeCommandExecutor executor(*device_);
        const auto& cmdBuffer = executor.cmdBuffer.getHandle();

        image.transitionLayoutDirect(
            cmdBuffer,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            0,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT);
        EXPECT_EQ(image.getLayout(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        image.copyFrom(cmdBuffer, stagingBuffer, 0, 1);

        image.transitionLayoutDirect(
            cmdBuffer,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT);
        EXPECT_EQ(image.getLayout(), VK_IMAGE_LAYOUT_GENERAL);

        image.transitionLayoutDirect(
            cmdBuffer,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT);
        EXPECT_EQ(image.getLayout(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        image.copyTo(cmdBuffer, downloadBuffer, 0, 1);
    }

    const std::span dataSpan(downloadBuffer.getHostVisibleData<uint8_t>(), downloadBuffer.getSize());
    EXPECT_THAT(dataSpan, ElementsAreArray(pixelData));
}

} // namespace
} // namespace crisp