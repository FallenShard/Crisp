#include <Crisp/Lights/EnvironmentLight.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderPasses/CubeMapRenderPass.hpp>
#include <Crisp/Renderer/RenderPasses/TexturePass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

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

    auto cubeMapPass = createTexturePass(device, {cubeMapSize, cubeMapSize}, cubeMap->getFormat());

    std::vector<std::unique_ptr<VulkanPipeline>> cubeMapPipelines(1);
    std::vector<std::unique_ptr<VulkanFramebuffer>> cubeMapFramebuffers(kCubeMapFaceCount);
    std::vector<std::unique_ptr<VulkanImageView>> cubeMapImageViews(kCubeMapFaceCount);
    for (uint32_t i = 0; i < kCubeMapFaceCount; ++i) {
        if (i == 0) {
            cubeMapPipelines[i] = renderer->createPipeline("EquirectToCube.json", *cubeMapPass, i);
        }
        cubeMapImageViews[i] = createView(device, *cubeMap, VK_IMAGE_VIEW_TYPE_2D, i, 1, 0, 1);
        cubeMapFramebuffers[i] = std::make_unique<VulkanFramebuffer>(
            device,
            cubeMapPass->getHandle(),
            VkExtent2D{cubeMapSize, cubeMapSize},
            std::vector<VkImageView>{cubeMapImageViews[i]->getHandle()});
    }

    const VertexLayoutDescription vertexLayout = {{VertexAttribute::Position}};
    const auto unitCube = createGeometry(*renderer, createCubeMesh(), vertexLayout);
    auto sampler = std::make_unique<VulkanSampler>(
        renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f);
    auto cubeMapMaterial = std::make_unique<Material>(cubeMapPipelines[0].get());
    cubeMapMaterial->writeDescriptor(0, 0, equirectMap.getView().getDescriptorInfo(sampler.get()));
    renderer->getDevice().flushDescriptorUpdates();

    device.getGeneralQueue().submitAndWait(
        [&unitCube, &cubeMapPipelines, &cubeMapPass, &cubeMapMaterial, &cubeMapFramebuffers, &cubeMap](
            const VkCommandBuffer cmdBuffer) {
            VulkanCommandEncoder commandEncoder(cmdBuffer);

            commandEncoder.insertBarrier(kTransferWrite >> (kIndexInputRead | kVertexInputRead));
            commandEncoder.transitionLayout(
                *cubeMap,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                kNullStage >> kFragmentRead,
                cubeMap->getFirstMipRange());

            const glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
            const std::array<glm::mat4, kCubeMapFaceCount> captureViews = {
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

            for (uint32_t i = 0; i < kCubeMapFaceCount; i++) {
                commandEncoder.beginRenderPass(*cubeMapPass, *cubeMapFramebuffers[i]);

                glm::mat4 MVP = captureProjection * captureViews[i];
                std::vector<char> pushConst(sizeof(glm::mat4));
                memcpy(pushConst.data(), glm::value_ptr(MVP), sizeof(glm::mat4));

                commandEncoder.bindPipeline(*cubeMapPipelines[0]);
                cubeMapPipelines[0]->getPipelineLayout()->setPushConstants(cmdBuffer, pushConst.data());

                cubeMapMaterial->bind(cmdBuffer);
                unitCube.bindAndDraw(cmdBuffer);
                commandEncoder.endRenderPass(*cubeMapPass);
                cubeMap->setImageLayout(
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = i,
                        .layerCount = 1,
                    });
            }
            cubeMap->transitionLayout(
                cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, kCubeMapFaceCount, 0, 1, kColorWrite >> kTransferRead);
            cubeMap->buildMipmaps(cmdBuffer);
            cubeMap->transitionLayout(
                cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentSampledRead);
        });
    return cubeMap;
}

std::unique_ptr<VulkanImage> integrateBrdfLut(Renderer* renderer) {
    auto& device = renderer->getDevice();

    auto brdfLut = std::make_unique<VulkanImage>(
        device,
        VulkanImageDescription{
            .format = VK_FORMAT_R16G16_SFLOAT,
            .extent = {512, 512, 1},
            .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        });
    auto view = createView(device, *brdfLut, VK_IMAGE_VIEW_TYPE_2D);
    auto texPass = createTexturePass(renderer->getDevice(), VkExtent2D{512, 512}, VK_FORMAT_R16G16_SFLOAT);

    auto framebuffer = std::make_unique<VulkanFramebuffer>(
        device, texPass->getHandle(), view->getImage().getExtent2D(), std::vector<VkImageView>{view->getHandle()});

    std::vector<std::unique_ptr<VulkanFramebuffer>> cubeMapFramebuffers(kCubeMapFaceCount);

    auto pipeline = renderer->createPipeline("BrdfLut.json", *texPass, 0);

    renderer->getDevice().getGeneralQueue().submitAndWait(
        [renderer, &pipeline, &texPass, &framebuffer, &brdfLut](VkCommandBuffer cmdBuffer) {
            VulkanCommandEncoder commandEncoder(cmdBuffer);
            commandEncoder.transitionLayout(
                *brdfLut, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kNullStage >> kFragmentRead);
            commandEncoder.beginRenderPass(*texPass, *framebuffer);

            commandEncoder.bindPipeline(*pipeline);
            renderer->drawFullScreenQuad(cmdBuffer);

            commandEncoder.endRenderPass(*texPass);
            brdfLut->setImageLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, brdfLut->getFullRange());
            commandEncoder.transitionLayout(
                *brdfLut, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kColorWrite >> kFragmentSampledRead);
        });

    return brdfLut;
}

} // namespace crisp
