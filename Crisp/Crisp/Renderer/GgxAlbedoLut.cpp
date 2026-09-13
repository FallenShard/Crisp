#include <Crisp/Renderer/GgxAlbedoLut.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Image/Io/Exr.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>
#include <Crisp/Vulkan/VulkanStagingBuffer.hpp>

namespace crisp {

std::unique_ptr<VulkanImage> loadGgxAlbedoLut(VulkanDevice& device, const std::filesystem::path& path) {
    const auto exr = loadExr(path).unwrap();

    CRISP_CHECK_EQ(exr.width, kGgxAlbedoLutExtent);
    CRISP_CHECK_EQ(exr.height, kGgxAlbedoLutExtent);
    CRISP_CHECK_EQ(exr.channelCount, 4, "GgxAlbedoLut.exr must hold R, G, B and A.");

    auto lut = std::make_unique<VulkanImage>(
        device,
        VulkanImageDescription{
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent = {kGgxAlbedoLutExtent, kGgxAlbedoLutExtent, 1},
            .mipLevelCount = 1,
            .layerCount = 1,
            .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        });
    device.setObjectName(*lut, "GGX Directional Albedo LUT");

    const auto staging = createStagingBuffer(device, exr.pixelData.data(), exr.pixelData.size() * sizeof(float));

    submitAndWait(device.getGeneralQueue(), [&staging, &lut](const VulkanCommandEncoder& encoder) {
        encoder.transitionLayout(*lut, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kNullStage >> kTransferWrite);
        encoder.copyBufferToImage(*staging, *lut);
        encoder.transitionLayout(*lut, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kAllShaderRead);
    });

    return lut;
}

} // namespace crisp
