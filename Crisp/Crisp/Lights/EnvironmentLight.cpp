#include <Crisp/Lights/EnvironmentLight.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderPasses/CubeMapRenderPass.hpp>
#include <Crisp/Renderer/RenderPasses/TexturePass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

namespace crisp {
EnvironmentLight::EnvironmentLight(Renderer& renderer, ImageBasedLightingData&& iblData) {
    auto equirectMap =
        createVulkanImage(renderer, iblData.equirectangularEnvironmentMap, VK_FORMAT_R32G32B32A32_SFLOAT);
    std::tie(m_cubeMap, m_cubeMapView) =
        convertEquirectToCubeMap(&renderer, createView(*equirectMap, VK_IMAGE_VIEW_TYPE_2D));

    m_diffuseEnvironmentMap =
        createVulkanCubeMap(renderer, {std::move(iblData.diffuseIrradianceCubeMap)}, VK_FORMAT_R32G32B32A32_SFLOAT);
    m_diffuseEnvironmentMapView = createView(*m_diffuseEnvironmentMap, VK_IMAGE_VIEW_TYPE_CUBE);

    m_specularEnvironmentMap =
        createVulkanCubeMap(renderer, iblData.specularReflectanceMapMipLevels, VK_FORMAT_R32G32B32A32_SFLOAT);
    m_specularEnvironmentMapView = createView(*m_specularEnvironmentMap, VK_IMAGE_VIEW_TYPE_CUBE);
}

void EnvironmentLight::update(Renderer& renderer, ImageBasedLightingData&& iblData) {
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
    Renderer* renderer, std::shared_ptr<VulkanImageView> equirectMapView) {
    const auto cubeMapSize = equirectMapView->getImage().getHeight() / 2;

    const auto mipmapCount = Image::getMipLevels(cubeMapSize, cubeMapSize);
    const auto additionalFlags = mipmapCount == 1 ? 0 : VK_IMAGE_USAGE_TRANSFER_DST_BIT; // for mipmap transfers

    auto cubeMapRenderTarget =
        RenderTargetBuilder()
            .setFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
            .setLayerAndMipLevelCount(kCubeMapFaceCount, mipmapCount)
            .setCreateFlags(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
            .configureColorRenderTarget(
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                additionalFlags)
            .setSize({cubeMapSize, cubeMapSize}, false)
            .create(renderer->getDevice());

    auto cubeMapPass = createCubeMapPass(renderer->getDevice(), cubeMapRenderTarget.get(), {cubeMapSize, cubeMapSize});
    renderer->updateInitialLayouts(*cubeMapPass);
    std::vector<std::unique_ptr<VulkanPipeline>> cubeMapPipelines(kCubeMapFaceCount);
    for (uint32_t i = 0; i < kCubeMapFaceCount; ++i) {
        cubeMapPipelines[i] = renderer->createPipeline("EquirectToCube.json", *cubeMapPass, i);
    }

    const VertexLayoutDescription vertexLayout = {{VertexAttribute::Position}};
    auto unitCube = std::make_unique<Geometry>(*renderer, createCubeMesh(flatten(vertexLayout)), vertexLayout);
    auto sampler = std::make_unique<VulkanSampler>(
        renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f);
    auto cubeMapMaterial = std::make_unique<Material>(cubeMapPipelines[0].get());
    cubeMapMaterial->writeDescriptor(0, 0, *equirectMapView, sampler.get());
    renderer->getDevice().flushDescriptorUpdates();

    renderer->enqueueResourceUpdate(
        [&unitCube, &cubeMapPipelines, &cubeMapPass, &cubeMapMaterial](VkCommandBuffer cmdBuffer) {
            // VkMemoryBarrier barrier{};
            // barrier.sType = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            // barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            // barrier.dstAccessMask = VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            // VkMemoryBarrier barrier2{};
            // barrier.sType = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            // barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            // barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            // std::array<VkMemoryBarrier, 2> barriers = {
            //     {barrier, barrier2}
            // };
            // vkCmdPipelineBarrier(
            //     cmdBuffer,
            //     VK_PIPELINE_STAGE_TRANSFER_BIT,
            //     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            //     0,
            //     static_cast<uint32_t>(barriers.size()),
            //     barriers.data(),
            //     0,
            //     nullptr,
            //     0,
            //     nullptr);

            std::array<VkBufferMemoryBarrier, 2> barriers{};
            VkBufferMemoryBarrier& barrier = barriers[0];
            barrier.buffer = unitCube->getIndexBuffer()->getHandle();
            barrier.sType = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            barrier.offset = 0;
            barrier.size = unitCube->getIndexBuffer()->getSize();
            VkBufferMemoryBarrier& barrier2 = barriers[1];
            barrier2.buffer = unitCube->getVertexBuffer()->getHandle();
            barrier2.sType = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier2.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            barrier2.offset = 0;
            barrier2.size = unitCube->getVertexBuffer()->getSize();
            vkCmdPipelineBarrier(
                cmdBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                0,
                nullptr,
                static_cast<uint32_t>(barriers.size()),
                barriers.data(),
                0,
                nullptr);

            const glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
            const std::array<glm::mat4, kCubeMapFaceCount> captureViews = {
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

            cubeMapPass->begin(cmdBuffer, 0, VK_SUBPASS_CONTENTS_INLINE);

            for (uint32_t i = 0; i < kCubeMapFaceCount; i++) {
                glm::mat4 MVP = captureProjection * captureViews[i];
                std::vector<char> pushConst(sizeof(glm::mat4));
                memcpy(pushConst.data(), glm::value_ptr(MVP), sizeof(glm::mat4));

                cubeMapPipelines[i]->bind(cmdBuffer);
                cubeMapPipelines[i]->getPipelineLayout()->setPushConstants(cmdBuffer, pushConst.data());

                cubeMapMaterial->bind(0, cmdBuffer);
                unitCube->bindAndDraw(cmdBuffer);

                if (i < 5) {
                    cubeMapPass->nextSubpass(cmdBuffer);
                }
            }

            cubeMapPass->end(cmdBuffer, 0);

            cubeMapPass->getRenderTarget(0).transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                0,
                6,
                0,
                1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
        });
    renderer->flushResourceUpdates(true);

    std::unique_ptr<VulkanImage> cubeMap = std::move(cubeMapRenderTarget->image);
    renderer->enqueueResourceUpdate([&cubeMap](VkCommandBuffer cmdBuffer) {
        cubeMap->buildMipmaps(cmdBuffer);
        cubeMap->transitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    });
    renderer->flushResourceUpdates(true);

    auto cubeMapView = createView(*cubeMap, VK_IMAGE_VIEW_TYPE_CUBE, 0, kCubeMapFaceCount, 0, cubeMap->getMipLevels());
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

    auto convPass = createCubeMapPass(renderer->getDevice(), cubeMapRenderTarget.get(), {cubeMapSize, cubeMapSize});
    renderer->updateInitialLayouts(*convPass);
    std::vector<std::unique_ptr<VulkanPipeline>> convPipelines(kCubeMapFaceCount);
    for (int i = 0; i < kCubeMapFaceCount; i++) {
        convPipelines[i] = renderer->createPipeline("ConvolveDiffuse.json", *convPass, i);
    }

    const VertexLayoutDescription vertexLayout = {{VertexAttribute::Position}};
    auto unitCube = std::make_unique<Geometry>(*renderer, createCubeMesh(flatten(vertexLayout)), vertexLayout);
    auto sampler = std::make_unique<VulkanSampler>(
        renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f);
    auto convMaterial = std::make_unique<Material>(convPipelines[0].get());
    convMaterial->writeDescriptor(0, 0, cubeMapView, sampler.get());
    renderer->getDevice().flushDescriptorUpdates();

    renderer->enqueueResourceUpdate([&unitCube, &convPipelines, &convPass, &convMaterial](VkCommandBuffer cmdBuffer) {
        glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 captureViews[] = {
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

        convPass->begin(cmdBuffer, 0, VK_SUBPASS_CONTENTS_INLINE);

        for (int i = 0; i < kCubeMapFaceCount; i++) {
            glm::mat4 MVP = captureProjection * captureViews[i];
            std::vector<char> pushConst(sizeof(glm::mat4));
            memcpy(pushConst.data(), glm::value_ptr(MVP), sizeof(glm::mat4));

            convPipelines[i]->bind(cmdBuffer);
            convPipelines[i]->getPipelineLayout()->setPushConstants(cmdBuffer, pushConst.data());

            convMaterial->bind(0, cmdBuffer);
            unitCube->bindAndDraw(cmdBuffer);

            if (i < kCubeMapFaceCount - 1) {
                convPass->nextSubpass(cmdBuffer);
            }
        }

        convPass->end(cmdBuffer, 0);
        convPass->getRenderTarget(0).transitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    });
    renderer->flushResourceUpdates(true);

    const std::string key = "envIrrMap";
    auto imageView = createView(convPass->getRenderTarget(0), VK_IMAGE_VIEW_TYPE_CUBE, 0, kCubeMapFaceCount);
    return std::make_pair(std::move(cubeMapRenderTarget->image), std::move(imageView));
}

std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> setupReflectEnvMap(
    Renderer* renderer, const VulkanImageView& cubeMapView, uint32_t cubeMapSize) {
    constexpr uint32_t kMipLevels = 5;
    // auto filteredCubeMap = createMipmapCubeMap(renderer, cubeMapSize, cubeMapSize, mipLevels);

    auto sampler = std::make_unique<VulkanSampler>(
        renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 5.0f);
    const VertexLayoutDescription vertexLayout = {{VertexAttribute::Position}};
    auto unitCube = std::make_unique<Geometry>(*renderer, createCubeMesh(flatten(vertexLayout)), vertexLayout);

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

    for (int i = 0; i < static_cast<int>(kMipLevels); i++) {
        float roughness = i / static_cast<float>(kMipLevels - 1);

        unsigned int w = static_cast<unsigned int>(cubeMapSize * std::pow(0.5, i));
        unsigned int h = static_cast<unsigned int>(cubeMapSize * std::pow(0.5, i));
        std::shared_ptr<VulkanRenderPass> prefilterPass =
            createCubeMapPass(renderer->getDevice(), environmentSpecularMap.get(), VkExtent2D{w, h}, i);
        renderer->updateInitialLayouts(*prefilterPass);

        std::vector<std::unique_ptr<VulkanPipeline>> filterPipelines(kCubeMapFaceCount);
        for (int j = 0; j < kCubeMapFaceCount; j++) {
            filterPipelines[j] = renderer->createPipeline("PrefilterSpecular.json", *prefilterPass, j);
        }

        std::shared_ptr filterMat = std::make_unique<Material>(filterPipelines[0].get());
        filterMat->writeDescriptor(0, 0, cubeMapView, sampler.get());
        renderer->getDevice().flushDescriptorUpdates();

        renderer->enqueueResourceUpdate(
            [renderer, &unitCube, &filterPipelines, prefilterPass, filterMat, i, w, h, roughness](
                VkCommandBuffer cmdBuffer) {
                glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
                glm::mat4 captureViews[] = {
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                    glm::lookAt(
                        glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                    glm::lookAt(
                        glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                    glm::lookAt(
                        glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

                prefilterPass->begin(cmdBuffer, 0, VK_SUBPASS_CONTENTS_INLINE);

                for (int j = 0; j < 6; j++) {
                    glm::mat4 MVP = captureProjection * captureViews[j];
                    std::vector<char> pushConst(sizeof(glm::mat4) + sizeof(float));
                    memcpy(pushConst.data(), glm::value_ptr(MVP), sizeof(glm::mat4));
                    memcpy(pushConst.data() + sizeof(glm::mat4), &roughness, sizeof(float));

                    filterPipelines[j]->bind(cmdBuffer);
                    filterPipelines[j]->getPipelineLayout()->setPushConstants(cmdBuffer, pushConst.data());

                    filterMat->bind(0, cmdBuffer);
                    unitCube->bindAndDraw(cmdBuffer);

                    if (j < 5) {
                        prefilterPass->nextSubpass(cmdBuffer);
                    }
                }

                prefilterPass->end(cmdBuffer, 0);
                for (uint32_t k = 0; k < 6; ++k) {
                    prefilterPass->getRenderTarget(0).transitionLayout(
                        cmdBuffer,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        prefilterPass->getAttachmentView(k, 0).getSubresourceRange(),
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                    /*prefilterPass->getRenderTarget(0).transitionLayout(
                        cmdBuffer,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        prefilterPass->getRenderTargetView(k, 0).getSubresourceRange(),
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT);*/
                }
            });
        renderer->flushResourceUpdates(true);

        /*renderer->enqueueResourceUpdate(
            [&environmentSpecularMap, i, &filteredCubeMap](VkCommandBuffer cmdBuffer)
            {
                filteredCubeMap->blit(cmdBuffer, *environmentSpecularMap->image, i);
            });
        renderer->flushResourceUpdates(true);*/
    }

    auto view =
        createView(*environmentSpecularMap->image, VK_IMAGE_VIEW_TYPE_CUBE, 0, kCubeMapFaceCount, 0, kMipLevels);
    return std::make_pair(std::move(environmentSpecularMap->image), std::move(view));
}

// coro::Task<std::unique_ptr<VulkanImage>> integrateBrdfLutTask(Renderer* renderer)
//{
//     std::shared_ptr<VulkanRenderPass> texPass =
//         createTexturePass(renderer->getDevice(), VkExtent2D{512, 512}, VK_FORMAT_R16G16_SFLOAT, false);
//     std::shared_ptr<VulkanPipeline> pipeline = renderer->createPipeline("BrdfLut.lua", *texPass, 0);
//
//     VkCommandBuffer cmdBuffer = co_await renderer->getNextCommandBuffer();
//     texPass->updateInitialLayouts(cmdBuffer);
//
//     texPass->begin(cmdBuffer, 0, VK_SUBPASS_CONTENTS_INLINE);
//
//     pipeline->bind(cmdBuffer);
//     renderer->drawFullScreenQuad(cmdBuffer);
//
//     texPass->end(cmdBuffer, 0);
//     texPass->getRenderTarget(0).transitionLayout(
//         cmdBuffer,
//         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
//         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
//
//     co_return texPass->extractRenderTarget(0);
// }

std::unique_ptr<VulkanImage> integrateBrdfLut(Renderer* renderer) {
    RenderTargetCache cache{};
    std::shared_ptr<VulkanRenderPass> texPass =
        createTexturePass(renderer->getDevice(), cache, VkExtent2D{512, 512}, VK_FORMAT_R16G16_SFLOAT, false);
    renderer->updateInitialLayouts(*texPass);
    std::shared_ptr<VulkanPipeline> pipeline = renderer->createPipeline("BrdfLut.json", *texPass, 0);

    renderer->enqueueResourceUpdate([renderer, pipeline, texPass](VkCommandBuffer cmdBuffer) {
        texPass->begin(cmdBuffer, 0, VK_SUBPASS_CONTENTS_INLINE);

        pipeline->bind(cmdBuffer);
        renderer->drawFullScreenQuad(cmdBuffer);

        texPass->end(cmdBuffer, 0);
        texPass->getRenderTarget(0).transitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    });
    renderer->flushResourceUpdates(true);

    return std::move(cache.extract("TexturePass")->image);
}

} // namespace crisp
