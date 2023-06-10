#include <Crisp/Scenes/OceanScene.hpp>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>

#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <imgui.h>

namespace crisp
{
namespace
{
auto logger = spdlog::stdout_color_st("OceanScene");

constexpr const char* kMainPass = "forwardPass";
constexpr const char* kSpectrumPass = "oscillationPass";
constexpr const char* kGeometryPass = "geometryPass";

constexpr int32_t N = 512;
const int logN = static_cast<int>(std::log2(N));
constexpr float Lx = 2.0f * N;
constexpr float kGravity = 9.81f;

constexpr float kGeometryPatchWorldSize = 20.0f;
constexpr uint32_t kInstanceCount = 64;

std::unique_ptr<VulkanPipeline> createComputePipeline(
    Renderer* renderer, const glm::uvec3& workGroupSize, const std::string& shaderName)
{
    const auto spirvContents =
        sl::readSpirvFile(renderer->getResourcesPath() / "Shaders" / (shaderName + ".spv")).unwrap();
    PipelineLayoutBuilder layoutBuilder(sl::reflectUniformMetadataFromSpirvShader(spirvContents).unwrap());

    auto layout = layoutBuilder.create(renderer->getDevice());

    const std::array<VkSpecializationMapEntry, 3> specEntries = {
  //                            id,               offset,             size
        VkSpecializationMapEntry{0, 0 * sizeof(uint32_t), sizeof(uint32_t)},
        VkSpecializationMapEntry{1, 1 * sizeof(uint32_t), sizeof(uint32_t)},
        VkSpecializationMapEntry{2, 2 * sizeof(uint32_t), sizeof(uint32_t)},
    };

    VkSpecializationInfo specInfo = {};
    specInfo.mapEntryCount = static_cast<uint32_t>(specEntries.size());
    specInfo.pMapEntries = specEntries.data();
    specInfo.dataSize = sizeof(workGroupSize);
    specInfo.pData = glm::value_ptr(workGroupSize);

    VkComputePipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.stage = createShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, renderer->getShaderModule(shaderName));
    pipelineInfo.stage.pSpecializationInfo = &specInfo;
    pipelineInfo.layout = layout->getHandle();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
    VkPipeline pipeline;
    vkCreateComputePipelines(renderer->getDevice().getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    return std::make_unique<VulkanPipeline>(
        renderer->getDevice(), pipeline, std::move(layout), VK_PIPELINE_BIND_POINT_COMPUTE, VertexLayout{});
}

VkImageMemoryBarrier createImageMemoryReadBarrier(const VkImage imageHandle, const uint32_t layerIdx)
{
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.image = imageHandle;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = layerIdx;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    return barrier;
}

} // namespace

OceanScene::OceanScene(Renderer* renderer, Window* window)
    : Scene(renderer, window)
    , m_oceanParams(createOceanParameters(N, Lx, 10.0f, 0.0f, 4.0f, 0.5f))
    , m_choppiness(0.0f)
{
    m_window->keyPressed += [this](Key key, int /*modifiers*/)
    {
        if (key == Key::Space)
            m_paused = !m_paused;
        else if (key == Key::F5)
            m_resourceContext->recreatePipelines();
    };

    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

    std::vector<std::vector<VertexAttributeDescriptor>> vertexFormat = {
        {VertexAttribute::Position}, {VertexAttribute::Normal}};
    TriangleMesh mesh = createGridMesh(flatten(vertexFormat), kGeometryPatchWorldSize, N);
    m_resourceContext->addGeometry(
        "ocean",
        std::make_unique<Geometry>(*m_renderer, mesh, vertexFormat, false, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    m_resourceContext->getGeometry("ocean")->setInstanceCount(kInstanceCount);

    auto displacementYImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
    auto displacementXImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
    auto displacementZImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
    auto normalXImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
    auto normalZImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
    auto spectrumImage = createInitialSpectrum();

    m_renderer->enqueueResourceUpdate(
        [dy = displacementYImage.get(),
         dx = displacementXImage.get(),
         dz = displacementZImage.get(),
         nx = normalXImage.get(),
         nz = normalZImage.get()](VkCommandBuffer cmdBuffer)
        {
            const auto transitionToGeneral = [cmdBuffer](VulkanImage& image)
            {
                image.transitionLayout(
                    cmdBuffer,
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            };
            transitionToGeneral(*dy);
            transitionToGeneral(*dx);
            transitionToGeneral(*dz);
            transitionToGeneral(*nx);
            transitionToGeneral(*nz);
        });

    auto& imageCache = m_resourceContext->imageCache;
    const auto addImage = [&imageCache](std::unique_ptr<VulkanImage> image, const std::string& name)
    {
        for (uint32_t i = 0; i < 2; ++i)
        {
            const std::string viewName{fmt::format("{}View{}", name, i)};
            imageCache.addImageView(viewName, createView(*image, VK_IMAGE_VIEW_TYPE_2D, i, 1));
        }
        imageCache.addImage(name, std::move(image));
    };
    addImage(std::move(displacementYImage), "fftImage");
    addImage(std::move(displacementXImage), "displacementX");
    addImage(std::move(displacementZImage), "displacementZ");
    addImage(std::move(normalXImage), "normalX");
    addImage(std::move(normalZImage), "normalZ");

    imageCache.addImageView("randImageView", createView(*spectrumImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1));
    imageCache.addImage("randImage", std::move(spectrumImage));

    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), MaxAnisotropy));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), MaxAnisotropy));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), MaxAnisotropy, 9.0f));

    auto& oscillationPass = m_renderGraph->addComputePass(kSpectrumPass);
    oscillationPass.workGroupSize = glm::ivec3(16, 16, 1);
    oscillationPass.numWorkGroups = computeWorkGroupCount(glm::uvec3(N, N, 1), oscillationPass.workGroupSize);
    oscillationPass.pipeline = createComputePipeline(m_renderer, oscillationPass.workGroupSize, "ocean-spectrum.comp");
    oscillationPass.material = std::make_unique<Material>(oscillationPass.pipeline.get());
    oscillationPass.material->writeDescriptor(
        0, 0, imageCache.getImageView("randImageView").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    oscillationPass.material->writeDescriptor(
        0, 1, imageCache.getImageView("fftImageView0").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    oscillationPass.material->writeDescriptor(
        0, 2, imageCache.getImageView("displacementXView0").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    oscillationPass.material->writeDescriptor(
        0, 3, imageCache.getImageView("displacementZView0").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    oscillationPass.material->writeDescriptor(
        0, 4, imageCache.getImageView("normalXView0").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    oscillationPass.material->writeDescriptor(
        0, 5, imageCache.getImageView("normalZView0").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

    oscillationPass.preDispatchCallback =
        [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
    {
        VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barrier.buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer()->getHandle();
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(
            cmdBuffer.getHandle(),
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0,
            nullptr,
            1,
            &barrier,
            0,
            nullptr);

        node.pipeline->setPushConstants(cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, m_oceanParams);
    };

    int layerToRead = applyFFT("fftImage");
    int ltr1 = applyFFT("displacementX");
    int ltr2 = applyFFT("displacementZ");

    int ln1 = applyFFT("normalX");
    int ln2 = applyFFT("normalZ");

    auto& geometryPass = m_renderGraph->addComputePass(kGeometryPass);
    geometryPass.workGroupSize = glm::ivec3(16, 16, 1);
    geometryPass.numWorkGroups = computeWorkGroupCount(glm::uvec3(N + 1, N + 1, 1), geometryPass.workGroupSize);
    geometryPass.pipeline = createComputePipeline(m_renderer, geometryPass.workGroupSize, "ocean-geometry.comp");
    geometryPass.material = std::make_unique<Material>(geometryPass.pipeline.get());
    geometryPass.material->writeDescriptor(
        0, 0, m_resourceContext->getGeometry("ocean")->getVertexBuffer(0)->createDescriptorInfo());
    geometryPass.material->writeDescriptor(
        0, 1, m_resourceContext->getGeometry("ocean")->getVertexBuffer(1)->createDescriptorInfo());
    geometryPass.material->writeDescriptor(
        0,
        2,
        imageCache.getImageView("fftImageView" + std::to_string(layerToRead))
            .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    geometryPass.material->writeDescriptor(
        0,
        3,
        imageCache.getImageView("displacementXView" + std::to_string(ltr1))
            .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    geometryPass.material->writeDescriptor(
        0,
        4,
        imageCache.getImageView("displacementZView" + std::to_string(ltr2))
            .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    geometryPass.material->writeDescriptor(
        0,
        5,
        imageCache.getImageView("normalXView" + std::to_string(ln1))
            .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    geometryPass.material->writeDescriptor(
        0,
        6,
        imageCache.getImageView("normalZView" + std::to_string(ln2))
            .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));

    geometryPass.preDispatchCallback =
        [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
    {
        std::array<VkBufferMemoryBarrier, 2> barriers{};
        barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer(0)->getHandle();
        barriers[0].offset = 0;
        barriers[0].size = VK_WHOLE_SIZE;

        barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barriers[1].buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer(1)->getHandle();
        barriers[1].offset = 0;
        barriers[1].size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(
            cmdBuffer.getHandle(),
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0,
            nullptr,
            2,
            barriers.data(),
            0,
            nullptr);

        struct GeometryUpdateParams
        {
            int patchSize{N};
            float patchWorldSize{};
            float choppiness{};
        };

        GeometryUpdateParams params{};
        params.patchWorldSize = kGeometryPatchWorldSize;
        params.choppiness = m_choppiness;
        node.pipeline->setPushConstants(cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, params);
    };

    m_renderGraph->addRenderPass(
        kMainPass,
        createForwardLightingPass(
            m_renderer->getDevice(), m_resourceContext->renderTargetCache, renderer->getSwapChainExtent()));

    m_renderGraph->addDependency(
        kGeometryPass,
        kMainPass,
        [this](const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer()->getHandle();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            cmdBuffer.insertBufferMemoryBarrier(
                barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

            barrier.buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer(1)->getHandle();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            cmdBuffer.insertBufferMemoryBarrier(
                barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
        });

    m_renderGraph->addDependency(kMainPass, "SCREEN", 0);
    m_renderGraph->sortRenderPasses().unwrap();
    m_renderGraph->printExecutionOrder();
    m_renderer->setSceneImageView(m_renderGraph->getNode(kMainPass).renderPass.get(), 0);

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 200);

    m_envLight = std::make_unique<EnvironmentLight>(
        *m_renderer,
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps/TableMountain").unwrap());
    m_skybox = m_envLight->createSkybox(
        *m_renderer, m_renderGraph->getRenderPass(kMainPass), imageCache.getSampler("linearClamp"));
    m_renderer->flushResourceUpdates(true);

    VulkanPipeline* pipeline =
        m_resourceContext->createPipeline("ocean", "ocean.lua", m_renderGraph->getRenderPass(kMainPass), 0);
    Material* material = m_resourceContext->createMaterial("ocean", pipeline);
    material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    material->writeDescriptor(
        0,
        1,
        imageCache.getImageView("fftImageView" + std::to_string(layerToRead))
            .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    material->writeDescriptor(
        0,
        2,
        imageCache.getImageView("displacementXView" + std::to_string(ltr1))
            .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    material->writeDescriptor(
        0,
        3,
        imageCache.getImageView("displacementZView" + std::to_string(ltr2))
            .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));

    material->writeDescriptor(
        0,
        4,
        imageCache.getImageView("normalXView" + std::to_string(ln1))
            .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    material->writeDescriptor(
        0,
        5,
        imageCache.getImageView("normalZView" + std::to_string(ln2))
            .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    material->writeDescriptor(1, 0, *m_resourceContext->getUniformBuffer("camera"));
    material->writeDescriptor(1, 1, m_envLight->getDiffuseMapView(), imageCache.getSampler("linearClamp"));
    material->writeDescriptor(1, 2, m_envLight->getSpecularMapView(), imageCache.getSampler("linearMipmap"));
    imageCache.addImageWithView("brdfLut", integrateBrdfLut(m_renderer));
    material->writeDescriptor(1, 3, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));

    auto node = std::make_unique<RenderNode>(*m_transformBuffer, m_transformBuffer->getNextIndex());
    node->transformPack->M = glm::mat4(1.0f);
    node->geometry = m_resourceContext->getGeometry("ocean");
    node->pass(kMainPass).material = material;
    node->pass(kMainPass).setPushConstants(kGeometryPatchWorldSize);
    m_renderNodes.emplace_back(std::move(node));

    m_renderer->getDevice().flushDescriptorUpdates();
}

OceanScene::~OceanScene() {}

void OceanScene::resize(int width, int height)
{
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(width, height);
    m_renderer->setSceneImageView(m_renderGraph->getNode(kMainPass).renderPass.get(), 0);
}

void OceanScene::update(float dt)
{
    m_cameraController->update(dt);
    const CameraParameters cameraParams = m_cameraController->getCameraParameters();

    m_transformBuffer->update(cameraParams.V, cameraParams.P);
    m_skybox->updateTransforms(cameraParams.V, cameraParams.P);

    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(cameraParams);

    if (!m_paused)
    {
        m_oceanParams.time += dt;
    }
}

void OceanScene::render()
{
    m_renderGraph->clearCommandLists();
    m_renderGraph->buildCommandLists(m_renderNodes);
    m_renderGraph->addToCommandLists(m_skybox->getRenderNode());
    m_renderGraph->executeCommandLists();
}

void OceanScene::renderGui()
{
    ImGui::Begin("Ocean Parameters");
    glm::vec2 windVelocity = m_oceanParams.windDirection * m_oceanParams.windSpeed;
    if (ImGui::SliderFloat("Wind Speed X", &windVelocity.x, 0.001f, 100.0f))
    {
        m_oceanParams.windSpeed = glm::length(windVelocity);
        m_oceanParams.windDirection = windVelocity / m_oceanParams.windSpeed;
        m_oceanParams.Lw = m_oceanParams.windSpeed * m_oceanParams.windSpeed / kGravity;
    }
    if (ImGui::SliderFloat("Wind Speed Z", &windVelocity.y, 0.001f, 100.0f))
    {
        m_oceanParams.windSpeed = glm::length(windVelocity);
        m_oceanParams.windDirection = windVelocity / m_oceanParams.windSpeed;
        m_oceanParams.Lw = m_oceanParams.windSpeed * m_oceanParams.windSpeed / kGravity;
    }
    ImGui::SliderFloat("Amplitude", &m_oceanParams.A, 0.0f, 1000.0f);
    ImGui::SliderFloat("Small Waves", &m_oceanParams.smallWaves, 0.0f, 10.0f);
    ImGui::SliderFloat("Choppiness", &m_choppiness, 0.0f, 10.0f);
    ImGui::End();
}

std::unique_ptr<VulkanImage> OceanScene::createInitialSpectrum()
{
    const auto oceanSpectrum{createOceanSpectrum(0, m_oceanParams)};

    auto image = createStorageImage(m_renderer->getDevice(), 1, N, N, VK_FORMAT_R32G32B32A32_SFLOAT);
    std::shared_ptr<VulkanBuffer> buffer = createStagingBuffer(m_renderer->getDevice(), oceanSpectrum);
    m_renderer->enqueueResourceUpdate(
        [img = image.get(), stagingBuffer = buffer](VkCommandBuffer cmdBuffer)
        {
            img->transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);

            img->copyFrom(cmdBuffer, *stagingBuffer, 0, 1);

            img->transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });

    return image;
}

int OceanScene::applyFFT(std::string image)
{
    struct BitReversalPushConstants
    {
        int32_t reversalDirection; // 0 is horizontal, 1 is vertical.
        int32_t passCount;         // Logarithm of the patch discretization size.
    };

    struct IFFTPushConstants
    {
        int32_t passIdx; // Index of the current pass.
        int32_t N;       // Number of elements in the array.
    };

    auto& imageCache = m_resourceContext->imageCache;
    int32_t imageLayerRead = 0;
    int32_t imageLayerWrite = 1;
    {
        const std::string imageViewRead = fmt::format("{}View{}", image, imageLayerRead);
        const std::string imageViewWrite = fmt::format("{}View{}", image, imageLayerWrite);

        auto& bitReversePass = m_renderGraph->addComputePass(image + "BitReversePass0");
        bitReversePass.workGroupSize = glm::ivec3(16, 16, 1);
        bitReversePass.numWorkGroups = glm::ivec3(N / 16, N / 16, 1);
        bitReversePass.pipeline =
            createComputePipeline(m_renderer, bitReversePass.workGroupSize, "ocean-reverse-bits.comp");
        bitReversePass.material = std::make_unique<Material>(bitReversePass.pipeline.get());
        bitReversePass.material->writeDescriptor(
            0, 0, imageCache.getImageView(imageViewRead).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        bitReversePass.material->writeDescriptor(
            0, 1, imageCache.getImageView(imageViewWrite).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

        bitReversePass.preDispatchCallback =
            [this, image](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            node.pipeline->setPushConstants(
                cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, BitReversalPushConstants{0, logN});
        };
        m_renderGraph->addDependency(
            kSpectrumPass,
            image + "BitReversePass0",
            [this, image, imageLayerRead](
                const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
            {
                cmdBuffer.insertImageMemoryBarrier(
                    createImageMemoryReadBarrier(
                        m_resourceContext->imageCache.getImage(image).getHandle(), imageLayerRead),
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            });
        logger->info("{} R: {} W: {}", image + "BitReversePass0", imageLayerRead, imageLayerWrite);

        std::swap(imageLayerRead, imageLayerWrite);
    }

    // Horizontal IFFT passes.
    for (int i = 0; i < logN; ++i)
    {
        const std::string imageViewRead = fmt::format("{}View{}", image, imageLayerRead);
        const std::string imageViewWrite = fmt::format("{}View{}", image, imageLayerWrite);

        std::string name = image + "TrueFFTPass" + std::to_string(i);
        auto& fftPass = m_renderGraph->addComputePass(name);
        fftPass.workGroupSize = glm::ivec3(16, 16, 1);
        fftPass.numWorkGroups = glm::ivec3(N / 2 / 16, N / 16, 1);
        fftPass.pipeline = createComputePipeline(m_renderer, fftPass.workGroupSize, "ifft-hori.comp");
        fftPass.material = std::make_unique<Material>(fftPass.pipeline.get());
        fftPass.material->writeDescriptor(
            0, 0, imageCache.getImageView(imageViewRead).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        fftPass.material->writeDescriptor(
            0, 1, imageCache.getImageView(imageViewWrite).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

        fftPass.preDispatchCallback =
            [this, i](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            node.pipeline->setPushConstants(
                cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, IFFTPushConstants{i + 1, N});
        };

        logger->info("{} R: {} W: {}", name, imageLayerRead, imageLayerWrite);

        if (i == 0)
        {
            m_renderGraph->addDependency(
                image + "BitReversePass0",
                name,
                [this, image, imageLayerRead](
                    const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
                {
                    cmdBuffer.insertImageMemoryBarrier(
                        createImageMemoryReadBarrier(
                            m_resourceContext->imageCache.getImage(image).getHandle(), imageLayerRead),
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                });
        }

        if (i > 0)
        {
            std::string prevName = image + "TrueFFTPass" + std::to_string(i - 1);
            m_renderGraph->addDependency(
                prevName,
                name,
                [this, image, imageLayerRead](
                    const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
                {
                    cmdBuffer.insertImageMemoryBarrier(
                        createImageMemoryReadBarrier(
                            m_resourceContext->imageCache.getImage(image).getHandle(), imageLayerRead),
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                });
        }

        std::swap(imageLayerRead, imageLayerWrite);
    }

    {
        const std::string imageViewRead = fmt::format("{}View{}", image, imageLayerRead);
        const std::string imageViewWrite = fmt::format("{}View{}", image, imageLayerWrite);
        auto& bitReversePass2 = m_renderGraph->addComputePass(image + "BitReversePass1");
        bitReversePass2.workGroupSize = glm::ivec3(16, 16, 1);
        bitReversePass2.numWorkGroups = glm::ivec3(N / 16, N / 16, 1);
        bitReversePass2.pipeline =
            createComputePipeline(m_renderer, bitReversePass2.workGroupSize, "ocean-reverse-bits.comp");
        bitReversePass2.material = std::make_unique<Material>(bitReversePass2.pipeline.get());
        bitReversePass2.material->writeDescriptor(
            0, 0, imageCache.getImageView(imageViewRead).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        bitReversePass2.material->writeDescriptor(
            0, 1, imageCache.getImageView(imageViewWrite).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

        bitReversePass2.preDispatchCallback =
            [this, image](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            node.pipeline->setPushConstants(
                cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, BitReversalPushConstants{1, logN});
        };

        logger->info("{} R: {} W: {}", image + "BitReversePass1", imageLayerRead, imageLayerWrite);

        m_renderGraph->addDependency(
            image + "TrueFFTPass" + std::to_string(logN - 1),
            image + "BitReversePass1",
            [this, image, imageLayerRead](
                const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
            {
                cmdBuffer.insertImageMemoryBarrier(
                    createImageMemoryReadBarrier(
                        m_resourceContext->imageCache.getImage(image).getHandle(), imageLayerRead),
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            });
        std::swap(imageLayerRead, imageLayerWrite);
    }

    // Vertical IFFT pass.
    std::string finalImageView;
    for (int i = 0; i < logN; ++i)
    {
        const std::string imageViewRead = fmt::format("{}View{}", image, imageLayerRead);
        const std::string imageViewWrite = fmt::format("{}View{}", image, imageLayerWrite);

        std::string name = image + "TrueFFTPassVert" + std::to_string(i);
        auto& fftPass = m_renderGraph->addComputePass(name);
        fftPass.workGroupSize = glm::ivec3(16, 16, 1);
        fftPass.numWorkGroups = glm::ivec3(N / 16, N / 16 / 2, 1);
        fftPass.pipeline = createComputePipeline(m_renderer, fftPass.workGroupSize, "ifft-vert.comp");
        fftPass.material = std::make_unique<Material>(fftPass.pipeline.get());
        fftPass.material->writeDescriptor(
            0, 0, imageCache.getImageView(imageViewRead).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        fftPass.material->writeDescriptor(
            0, 1, imageCache.getImageView(imageViewWrite).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

        logger->info("{} R: {} W: {}", name, imageLayerRead, imageLayerWrite);

        fftPass.preDispatchCallback =
            [this, i](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            node.pipeline->setPushConstants(
                cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, IFFTPushConstants{i + 1, N});
        };

        if (i == logN - 1)
        {
            finalImageView = imageViewWrite;
            m_renderGraph->addDependency(
                name,
                kGeometryPass,
                [this, image, imageLayerWrite](
                    const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
                {
                    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.image = m_resourceContext->imageCache.getImage(image).getHandle();
                    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseArrayLayer = imageLayerWrite;
                    barrier.subresourceRange.layerCount = 1;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;
                    cmdBuffer.insertImageMemoryBarrier(
                        barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
                });
        }
        else if (i == 0)
        {
            m_renderGraph->addDependency(
                image + "BitReversePass1",
                name,
                [this, image, imageLayerRead](
                    const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
                {
                    cmdBuffer.insertImageMemoryBarrier(
                        createImageMemoryReadBarrier(
                            m_resourceContext->imageCache.getImage(image).getHandle(), imageLayerRead),
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                });
        }

        if (i > 0)
        {
            std::string prevName = image + "TrueFFTPassVert" + std::to_string(i - 1);
            m_renderGraph->addDependency(
                prevName,
                name,
                [this, image, imageLayerRead](
                    const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
                {
                    cmdBuffer.insertImageMemoryBarrier(
                        createImageMemoryReadBarrier(
                            m_resourceContext->imageCache.getImage(image).getHandle(), imageLayerRead),
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                });
        }

        std::swap(imageLayerRead, imageLayerWrite);
    }

    return imageLayerRead;
}

} // namespace crisp