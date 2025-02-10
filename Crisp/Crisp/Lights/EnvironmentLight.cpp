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
    auto equirectMap = createVulkanImage(renderer, iblData.equirectangularEnvironmentMap, VK_FORMAT_R32G32B32A32_SFLOAT);
    std::tie(m_cubeMap, m_cubeMapView) =
        convertEquirectToCubeMap(&renderer, *createView(renderer.getDevice(), *equirectMap, VK_IMAGE_VIEW_TYPE_2D));

    m_diffuseEnvironmentMap = createVulkanCubeMap(
        renderer,
        std::span<const std::vector<Image>>(&iblData.diffuseIrradianceCubeMap, 1),
        VK_FORMAT_R32G32B32A32_SFLOAT);
    m_diffuseEnvironmentMapView = createView(renderer.getDevice(), *m_diffuseEnvironmentMap, VK_IMAGE_VIEW_TYPE_CUBE);

    m_specularEnvironmentMap =
        createVulkanCubeMap(renderer, iblData.specularReflectanceMapMipLevels, VK_FORMAT_R32G32B32A32_SFLOAT);
    m_specularEnvironmentMapView = createView(renderer.getDevice(), *m_specularEnvironmentMap, VK_IMAGE_VIEW_TYPE_CUBE);
}

void EnvironmentLight::update(Renderer& renderer, const ImageBasedLightingData& iblData) {
    /* auto equirectMap =
         createVulkanImage(renderer, iblData.equirectangularEnvironmentMap, VK_FORMAT_R32G32B32A32_SFLOAT);
     std::tie(m_cubeMap, m_cubeMapView) =
         convertEquirectToCubeMap(&renderer, createView(*equirectMap, VK_IMAGE_VIEW_TYPE_2D), 1024);*/

    updateCubeMap(*m_diffuseEnvironmentMap, renderer, iblData.diffuseIrradianceCubeMap);
    for (uint32_t i = 0; i < iblData.specularReflectanceMapMipLevels.size(); ++i) {
        updateCubeMap(*m_specularEnvironmentMap, renderer, iblData.specularReflectanceMapMipLevels[i], i);
    }
}

std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> convertEquirectToCubeMap(
    Renderer* renderer, const VulkanImageView& equirectMapView) {
    auto& device = renderer->getDevice();
    const auto cubeMapSize = equirectMapView.getImage().getHeight() / 2;

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
    device.getDebugMarker().setObjectName(*cubeMap, "CubeMap");

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
    cubeMapMaterial->writeDescriptor(0, 0, equirectMapView.getDescriptorInfo(sampler.get()));
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

    auto cubeMapView =
        createView(device, *cubeMap, VK_IMAGE_VIEW_TYPE_CUBE, 0, kCubeMapFaceCount, 0, cubeMap->getMipLevels());
    return std::make_pair(std::move(cubeMap), std::move(cubeMapView));
}

std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> setupDiffuseEnvMap(
    Renderer* renderer, const VulkanImageView& cubeMapView, uint32_t cubeMapSize) {
    auto cubeMapRenderTarget =
        RenderTargetBuilder()
            .setFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
            .setLayerAndMipLevelCount(6)
            .setCreateFlags(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
            .configureColorRenderTarget(
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            .setSize({cubeMapSize, cubeMapSize}, false)
            .create(renderer->getDevice());

    // auto convPass = createCubeMapPass(renderer->getDevice(), cubeMapRenderTarget.get(), {cubeMapSize, cubeMapSize});
    auto convPass = createCubeMapPass(renderer->getDevice(), {cubeMapSize, cubeMapSize});
    renderer->updateInitialLayouts(*convPass);
    std::vector<std::unique_ptr<VulkanPipeline>> convPipelines(kCubeMapFaceCount);
    for (uint32_t i = 0; i < kCubeMapFaceCount; i++) {
        convPipelines[i] = renderer->createPipeline("ConvolveDiffuse.json", *convPass, i);
    }

    const VertexLayoutDescription vertexLayout = {{VertexAttribute::Position}};
    const auto unitCube = createGeometry(*renderer, createCubeMesh(), vertexLayout);
    auto sampler = std::make_unique<VulkanSampler>(
        renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f);
    auto convMaterial = std::make_unique<Material>(convPipelines[0].get());
    convMaterial->writeDescriptor(0, 0, cubeMapView.getDescriptorInfo(sampler.get()));
    renderer->getDevice().flushDescriptorUpdates();

    renderer->enqueueResourceUpdate([&unitCube, &convPipelines, &convPass, &convMaterial](VkCommandBuffer cmdBuffer) {
        convPass->begin(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

        for (uint32_t i = 0; i < kCubeMapFaceCount; i++) {
            const glm::mat4 MVP = kCubeFaceProjection * kCubeMapViews[i];
            std::array<char, sizeof(glm::mat4)> pushConst{};
            memcpy(pushConst.data(), glm::value_ptr(MVP), sizeof(glm::mat4));

            convPipelines[i]->bind(cmdBuffer);
            convPipelines[i]->getPipelineLayout()->setPushConstants(cmdBuffer, pushConst.data());

            convMaterial->bind(cmdBuffer);
            unitCube.bindAndDraw(cmdBuffer);

            if (i < kCubeMapFaceCount - 1) {
                convPass->nextSubpass(cmdBuffer);
            }
        }

        convPass->end(cmdBuffer);
        convPass->getRenderTarget(0).transitionLayout(
            cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kColorWrite >> kFragmentSampledRead);
    });
    renderer->flushResourceUpdates(true);

    auto imageView =
        createView(renderer->getDevice(), convPass->getRenderTarget(0), VK_IMAGE_VIEW_TYPE_CUBE, 0, kCubeMapFaceCount);
    return std::make_pair(std::move(cubeMapRenderTarget->image), std::move(imageView));
}

std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> setupReflectEnvMap(
    Renderer* renderer, const VulkanImageView& cubeMapView, uint32_t cubeMapSize) {
    constexpr uint32_t kMipLevels = 5;

    auto sampler = std::make_unique<VulkanSampler>(
        renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 5.0f);
    const VertexLayoutDescription vertexLayout = {{VertexAttribute::Position}};
    const auto unitCube = createGeometry(*renderer, createCubeMesh(), vertexLayout);

    auto environmentSpecularMap =
        RenderTargetBuilder()
            .setFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
            .setLayerAndMipLevelCount(kCubeMapFaceCount, kMipLevels)
            .setCreateFlags(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
            .configureColorRenderTarget(
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .setSize({cubeMapSize, cubeMapSize}, false)
            .create(renderer->getDevice());

    for (int32_t i = 0; i < static_cast<int>(kMipLevels); i++) {
        const float roughness = static_cast<float>(i) / static_cast<float>(kMipLevels - 1);

        const auto w = static_cast<unsigned int>(cubeMapSize * std::pow(0.5, i));
        const auto h = static_cast<unsigned int>(cubeMapSize * std::pow(0.5, i));
        std::shared_ptr<VulkanRenderPass> prefilterPass = createCubeMapPass(renderer->getDevice(), VkExtent2D{w, h});
        // std::shared_ptr<VulkanRenderPass> prefilterPass =
        //     createCubeMapPass(renderer->getDevice(), environmentSpecularMap.get(), VkExtent2D{w, h}, i);
        renderer->updateInitialLayouts(*prefilterPass);

        std::vector<std::unique_ptr<VulkanPipeline>> filterPipelines(kCubeMapFaceCount);
        for (uint32_t j = 0; j < kCubeMapFaceCount; j++) {
            filterPipelines[j] = renderer->createPipeline("PrefilterSpecular.json", *prefilterPass, j);
        }

        std::shared_ptr filterMat = std::make_unique<Material>(filterPipelines[0].get());
        filterMat->writeDescriptor(0, 0, cubeMapView.getDescriptorInfo(sampler.get()));
        renderer->getDevice().flushDescriptorUpdates();

        renderer->enqueueResourceUpdate(
            [&unitCube, &filterPipelines, prefilterPass, filterMat, i, roughness](VkCommandBuffer cmdBuffer) {
                prefilterPass->begin(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

                for (uint32_t j = 0; j < kCubeMapViews.size(); j++) {
                    const glm::mat4 MVP = kCubeFaceProjection * kCubeMapViews[i];
                    std::array<char, sizeof(glm::mat4) + sizeof(float)> pushConst{};
                    memcpy(pushConst.data(), glm::value_ptr(MVP), sizeof(glm::mat4));
                    memcpy(pushConst.data() + sizeof(glm::mat4), &roughness, sizeof(float)); // NOLINT

                    filterPipelines[j]->bind(cmdBuffer);
                    filterPipelines[j]->getPipelineLayout()->setPushConstants(cmdBuffer, pushConst.data());

                    filterMat->bind(cmdBuffer);
                    unitCube.bindAndDraw(cmdBuffer);

                    if (j < kCubeMapFaceCount - 1) {
                        prefilterPass->nextSubpass(cmdBuffer);
                    }
                }

                prefilterPass->end(cmdBuffer);
                for (uint32_t k = 0; k < 6; ++k) {
                    prefilterPass->getRenderTarget(0).transitionLayout(
                        cmdBuffer,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        prefilterPass->getAttachmentView(k).getSubresourceRange(),
                        kColorWrite >> kFragmentSampledRead);
                }
            });
        renderer->flushResourceUpdates(true);
    }

    auto view = createView(
        renderer->getDevice(),
        *environmentSpecularMap->image,
        VK_IMAGE_VIEW_TYPE_CUBE,
        0,
        kCubeMapFaceCount,
        0,
        kMipLevels);
    return {std::move(environmentSpecularMap->image), std::move(view)};
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

    renderer->updateInitialLayouts(*texPass);
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
