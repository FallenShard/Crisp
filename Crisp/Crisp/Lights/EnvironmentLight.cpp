#include <Crisp/Lights/EnvironmentLight.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRasterizationPassDescriptor.hpp>

namespace crisp {
namespace {
const glm::mat4 kCubeFaceProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
const std::array<glm::mat4, kCubeMapFaceCount> kCubeMapViews = {
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
};

void beginColorRendering(
    const VulkanCommandEncoder& commandEncoder, const VkImageView imageView, const VkExtent2D renderArea) {
    const VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = imageView,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = VkClearValue{.color{{0.0f, 0.0f, 0.0f, 1.0f}}},
    };
    const VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {.offset = {0, 0}, .extent = renderArea},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
    };
    commandEncoder.beginRendering(renderingInfo);
    commandEncoder.setViewport(
        {0.0f, 0.0f, static_cast<float>(renderArea.width), static_cast<float>(renderArea.height), 0.0f, 1.0f});
    commandEncoder.setScissor({.offset = {0, 0}, .extent = renderArea});
}
} // namespace

EnvironmentLight::EnvironmentLight(Renderer& renderer, const ImageBasedLightingData& iblData) {
    m_cubeMap = convertEquirectToCubeMap(
        &renderer, *createVulkanImage(renderer, iblData.equirectangularEnvironmentMap, VK_FORMAT_R32G32B32A32_SFLOAT));
    m_diffuseEnvironmentMap = createVulkanCubeMap(
        renderer,
        std::span<const std::vector<Image>>(&iblData.diffuseIrradianceCubeMap, 1),
        VK_FORMAT_R32G32B32A32_SFLOAT);
    m_specularEnvironmentMap =
        createVulkanCubeMap(renderer, iblData.specularReflectanceMapMipLevels, VK_FORMAT_R32G32B32A32_SFLOAT);
}

void EnvironmentLight::update(Renderer& renderer, const ImageBasedLightingData& iblData) {
    m_cubeMap = convertEquirectToCubeMap(
        &renderer, *createVulkanImage(renderer, iblData.equirectangularEnvironmentMap, VK_FORMAT_R32G32B32A32_SFLOAT));

    updateCubeMap(*m_diffuseEnvironmentMap, renderer, iblData.diffuseIrradianceCubeMap);
    for (uint32_t i = 0; i < iblData.specularReflectanceMapMipLevels.size(); ++i) {
        updateCubeMap(*m_specularEnvironmentMap, renderer, iblData.specularReflectanceMapMipLevels[i], i);
    }
}

std::unique_ptr<VulkanImage> convertEquirectToCubeMap(Renderer* renderer, const VulkanImage& equirectMap) {
    auto& device = renderer->getDevice();
    const auto cubeMapSize = equirectMap.getHeight() / 2;

    const auto mipmapCount = Image::getMipLevels(cubeMapSize, cubeMapSize);
    const uint32_t additionalFlags = mipmapCount == 1 ? 0 : VK_IMAGE_USAGE_TRANSFER_DST_BIT; // for mipmap transfers

    auto cubeMap = std::make_unique<VulkanImage>(
        device,
        VulkanImageDescription{
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .extent = {cubeMapSize, cubeMapSize, 1},
            .mipLevelCount = mipmapCount,
            .layerCount = kCubeMapFaceCount,
            .usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | additionalFlags,
            .createFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        });
    device.setObjectName(*cubeMap, "CubeMap");

    const VkExtent2D cubeMapExtent{cubeMapSize, cubeMapSize};
    const VulkanRasterizationPassDescriptor rasterizationPassDescriptor{
        .colorAttachmentFormats = {cubeMap->getFormat()},
    };
    auto cubeMapPipeline = renderer->createPipeline("EquirectToCube.json", rasterizationPassDescriptor);
    std::vector<std::unique_ptr<VulkanImageView>> cubeMapImageViews(kCubeMapFaceCount);
    for (uint32_t i = 0; i < kCubeMapFaceCount; ++i) {
        cubeMapImageViews[i] = createView(device, *cubeMap, VK_IMAGE_VIEW_TYPE_2D, i, 1, 0, 1);
    }

    const VertexLayoutDescription vertexLayout = {{VertexAttribute::Position}};
    const auto unitCube = createGeometry(*renderer, createCubeMesh(), vertexLayout);
    auto sampler = std::make_unique<VulkanSampler>(
        renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f);
    auto cubeMapMaterial = std::make_unique<Material>(cubeMapPipeline.get());
    cubeMapMaterial->writeDescriptor(0, 0, equirectMap.getView().getDescriptorInfo(sampler.get()));
    renderer->getDevice().flushDescriptorUpdates();

    device.getGeneralQueue().submitAndWait(
        [&unitCube, &cubeMapPipeline, &cubeMapMaterial, &cubeMapImageViews, &cubeMap, cubeMapExtent](
            const VkCommandBuffer cmdBuffer) {
            VulkanCommandEncoder commandEncoder(cmdBuffer);

            commandEncoder.insertBarrier(kTransferWrite >> (kIndexInputRead | kVertexInputRead));
            for (uint32_t i = 0; i < kCubeMapFaceCount; ++i) {
                const VkImageSubresourceRange faceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = i,
                    .layerCount = 1,
                };
                commandEncoder.transitionLayout(
                    *cubeMap, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, kNullStage >> kColorWrite, faceRange);
                beginColorRendering(commandEncoder, cubeMapImageViews[i]->getHandle(), cubeMapExtent);

                const glm::mat4 MVP = kCubeFaceProjection * kCubeMapViews[i];
                std::vector<char> pushConst(sizeof(glm::mat4));
                memcpy(pushConst.data(), glm::value_ptr(MVP), sizeof(glm::mat4));

                commandEncoder.bindPipeline(*cubeMapPipeline);
                cubeMapPipeline->getPipelineLayout()->setPushConstants(cmdBuffer, pushConst.data());

                cubeMapMaterial->bind(cmdBuffer);
                unitCube.bindAndDraw(cmdBuffer);
                commandEncoder.endRendering();
            }
            cubeMap->transitionLayout(
                cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, kCubeMapFaceCount, 0, 1, kColorWrite >> kTransferRead);
            cubeMap->buildMipmaps(cmdBuffer, kNullStage);
            cubeMap->transitionLayout(
                cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentSampledRead);
        });
    return cubeMap;
}

std::unique_ptr<VulkanImage> integrateBrdfLut(Renderer* renderer) {
    auto& device = renderer->getDevice();
    constexpr VkExtent2D kBrdfLutExtent{512, 512};

    auto brdfLut = std::make_unique<VulkanImage>(
        device,
        VulkanImageDescription{
            .format = VK_FORMAT_R16G16_SFLOAT,
            .extent = {kBrdfLutExtent.width, kBrdfLutExtent.height, 1},
            .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        });
    auto view = createView(device, *brdfLut, VK_IMAGE_VIEW_TYPE_2D);
    const VulkanRasterizationPassDescriptor rasterizationPassDescriptor{
        .colorAttachmentFormats = {brdfLut->getFormat()},
    };
    auto pipeline = renderer->createPipeline("BrdfLut.json", rasterizationPassDescriptor);

    renderer->getDevice().getGeneralQueue().submitAndWait(
        [renderer, &pipeline, &view, &brdfLut](VkCommandBuffer cmdBuffer) {
            VulkanCommandEncoder commandEncoder(cmdBuffer);
            commandEncoder.transitionLayout(
                *brdfLut, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, kNullStage >> kColorWrite);
            beginColorRendering(commandEncoder, view->getHandle(), brdfLut->getExtent2D());

            commandEncoder.bindPipeline(*pipeline);
            renderer->drawFullScreenQuad(cmdBuffer);

            commandEncoder.endRendering();
            commandEncoder.transitionLayout(
                *brdfLut, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kColorWrite >> kFragmentSampledRead);
        });

    return brdfLut;
}

} // namespace crisp
