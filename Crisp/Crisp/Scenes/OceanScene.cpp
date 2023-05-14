#include "OceanScene.hpp"

#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Image/Io/Utils.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>

#include <Crisp/Lights/EnvironmentLighting.hpp>
#include <Crisp/Models/Ocean.hpp>
#include <Crisp/Models/Skybox.hpp>

#include <Crisp/GUI/Label.hpp>
#include <Crisp/GUI/Panel.hpp>
#include <Crisp/GUI/Slider.hpp>

#include <Crisp/Utils/LuaConfig.hpp>

#include <Crisp/Core/Logger.hpp>

#include <Crisp/Vulkan/VulkanDevice.hpp>

#include <imgui.h>

namespace crisp
{
namespace
{
auto logger = spdlog::stdout_color_st("OceanScene");

constexpr const char* MainPass = "forwardPass";
constexpr const char* OscillationPass = "oscillationPass";
constexpr const char* GeometryPass = "geometryPass";

constexpr int N = 256;
const int logN = static_cast<int>(std::log2(N));
constexpr float Lx = 2.0f * N;

OceanParameters OceanParams = createOceanParameters(N, Lx, 40.0f, 0.0f, 4.0f, 0.5f);
glm::vec2 WindVelocity{OceanParams.windDirection * OceanParams.windSpeed};
float Choppiness{0.5f};

std::unique_ptr<VulkanPipeline> createComputePipeline(
    Renderer* renderer,
    const glm::uvec3& workGroupSize,
    const PipelineLayoutBuilder& layoutBuilder,
    const std::string& shaderName)
{
    VulkanDevice& device = renderer->getDevice();
    auto layout = layoutBuilder.create(device);

    std::vector<VkSpecializationMapEntry> specEntries = {
  //   id,               offset,             size
        {0, 0 * sizeof(uint32_t), sizeof(uint32_t)},
        {1, 1 * sizeof(uint32_t), sizeof(uint32_t)},
        {2, 2 * sizeof(uint32_t), sizeof(uint32_t)}
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
    vkCreateComputePipelines(device.getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    return std::make_unique<VulkanPipeline>(
        device, pipeline, std::move(layout), VK_PIPELINE_BIND_POINT_COMPUTE, VertexLayout{});
}

std::unique_ptr<VulkanPipeline> createSpectrumPipeline(Renderer* renderer, const glm::uvec3& workGroupSize)
{
    PipelineLayoutBuilder layoutBuilder;
    layoutBuilder.defineDescriptorSet(
        0,
        false,
        {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}
    });

    layoutBuilder.addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(OceanParameters));
    return createComputePipeline(renderer, workGroupSize, layoutBuilder, "ocean-spectrum.comp");
}

std::unique_ptr<VulkanPipeline> createGeometryPipeline(Renderer* renderer, const glm::uvec3& workGroupSize)
{
    PipelineLayoutBuilder layoutBuilder;
    layoutBuilder.defineDescriptorSet(
        0,
        false,
        {
            {0,         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1,         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT}
    });

    layoutBuilder.addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(OceanParameters));
    return createComputePipeline(renderer, workGroupSize, layoutBuilder, "ocean-geometry.comp");
}

std::unique_ptr<VulkanPipeline> createBitReversalPipeline(Renderer* renderer, const glm::uvec3& workGroupSize)
{
    PipelineLayoutBuilder layoutBuilder;
    layoutBuilder.defineDescriptorSet(
        0,
        false,
        {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
    });

    layoutBuilder.addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(uint32_t) + sizeof(float));
    return createComputePipeline(renderer, workGroupSize, layoutBuilder, "ocean-reverse-bits.comp");
}

std::unique_ptr<VulkanPipeline> createIFFTPipeline(
    Renderer* renderer, const glm::uvec3& workGroupSize, const std::string& shaderName)
{
    PipelineLayoutBuilder layoutBuilder;
    layoutBuilder.defineDescriptorSet(
        0,
        false,
        {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
    });

    layoutBuilder.addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(int32_t) + sizeof(float));
    return createComputePipeline(renderer, workGroupSize, layoutBuilder, shaderName);
}

std::unique_ptr<VulkanImage> createStorageImage(
    VulkanDevice& device, uint32_t layerCount, uint32_t width, uint32_t height, VkFormat format)
{
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

bool paused = false;
} // namespace

OceanScene::OceanScene(Renderer* renderer, Application* app)
    : AbstractScene(app, renderer)
{
    m_app->getWindow()->keyPressed += [this](Key key, int /*modifiers*/)
    {
        if (key == Key::Space)
            paused = !paused;
        if (key == Key::F5)
            m_resourceContext->recreatePipelines();
    };

    m_cameraController = std::make_unique<FreeCameraController>(app->getWindow());
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

    std::vector<std::vector<VertexAttributeDescriptor>> vertexFormat = {
        {VertexAttribute::Position}, {VertexAttribute::Normal}};
    TriangleMesh mesh = createGridMesh(flatten(vertexFormat), 50.0, N - 1);
    m_resourceContext->addGeometry(
        "ocean",
        std::make_unique<Geometry>(*m_renderer, mesh, vertexFormat, false, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    m_resourceContext->getGeometry("ocean")->setInstanceCount(64);

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
            imageCache.addImageView(viewName, image->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1));
        }
        imageCache.addImage(name, std::move(image));
    };
    addImage(std::move(displacementYImage), "fftImage");
    addImage(std::move(displacementXImage), "displacementX");
    addImage(std::move(displacementZImage), "displacementZ");
    addImage(std::move(normalXImage), "normalX");
    addImage(std::move(normalZImage), "normalZ");

    imageCache.addImageView("randImageView", spectrumImage->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1));
    imageCache.addImage("randImage", std::move(spectrumImage));

    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), MaxAnisotropy));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), MaxAnisotropy));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), MaxAnisotropy, 9.0f));

    auto& oscillationPass = m_renderGraph->addComputePass(OscillationPass);
    oscillationPass.workGroupSize = glm::ivec3(16, 16, 1);
    oscillationPass.numWorkGroups = glm::ivec3(N / 16, N / 16, 1);
    oscillationPass.pipeline = createSpectrumPipeline(m_renderer, oscillationPass.workGroupSize);
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

        node.pipeline->setPushConstants(cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, OceanParams);
    };

    int layerToRead = applyFFT("fftImage");
    int ltr1 = applyFFT("displacementX");
    int ltr2 = applyFFT("displacementZ");

    int ln1 = applyFFT("normalX");
    int ln2 = applyFFT("normalZ");

    auto& geometryPass = m_renderGraph->addComputePass(GeometryPass);
    geometryPass.workGroupSize = glm::ivec3(16, 16, 1);
    geometryPass.numWorkGroups = glm::ivec3(N / 16, N / 16, 1);
    geometryPass.pipeline = createGeometryPipeline(m_renderer, geometryPass.workGroupSize);
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
            float choppiness{Choppiness};
        };

        GeometryUpdateParams params{};
        node.pipeline->setPushConstants(cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, params);
    };

    m_renderGraph->addRenderPass(
        MainPass,
        createForwardLightingPass(
            m_renderer->getDevice(), m_resourceContext->renderTargetCache, renderer->getSwapChainExtent()));

    m_renderGraph->addDependency(
        GeometryPass,
        MainPass,
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

    m_renderGraph->addDependency(MainPass, "SCREEN", 0);
    m_renderGraph->sortRenderPasses().unwrap();
    m_renderGraph->printExecutionOrder();
    m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 200);

    const auto iblData{
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps/TableMountain")};

    auto equirectMap =
        createVulkanImage(*m_renderer, iblData.equirectangularEnvironmentMap, VK_FORMAT_R32G32B32A32_SFLOAT);
    auto [cubeMap, cubeMapView] =
        convertEquirectToCubeMap(m_renderer, equirectMap->createView(VK_IMAGE_VIEW_TYPE_2D), 1024);

    auto diffEnvMap =
        createVulkanCubeMap(*m_renderer, {iblData.diffuseIrradianceCubeMap}, VK_FORMAT_R32G32B32A32_SFLOAT);
    auto diffEnvView = diffEnvMap->createView(VK_IMAGE_VIEW_TYPE_CUBE);

    auto specEnvMap =
        createVulkanCubeMap(*m_renderer, iblData.specularReflectanceMapMipLevels, VK_FORMAT_R32G32B32A32_SFLOAT);
    auto specEnvView = specEnvMap->createView(VK_IMAGE_VIEW_TYPE_CUBE);
    imageCache.addImageWithView("cubeMap", std::move(cubeMap), std::move(cubeMapView));
    imageCache.addImageWithView("diffEnvMap", std::move(diffEnvMap), std::move(diffEnvView));
    imageCache.addImageWithView("specEnvMap", std::move(specEnvMap), std::move(specEnvView));

    m_renderer->flushResourceUpdates(true);

    VulkanPipeline* pipeline =
        m_resourceContext->createPipeline("ocean", "ocean.lua", m_renderGraph->getRenderPass(MainPass), 0);
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
    material->writeDescriptor(1, 1, imageCache.getImageView("diffEnvMap"), imageCache.getSampler("linearClamp"));
    material->writeDescriptor(1, 2, imageCache.getImageView("specEnvMap"), imageCache.getSampler("linearMipmap"));
    imageCache.addImageWithView("brdfLut", integrateBrdfLut(m_renderer));
    material->writeDescriptor(1, 3, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));

    m_skybox = std::make_unique<Skybox>(
        m_renderer,
        m_renderGraph->getRenderPass(MainPass),
        imageCache.getImageView("cubeMap"),
        imageCache.getSampler("linearClamp"));

    for (int i = 0; i < 1; ++i)
    {
        for (int j = 0; j < 1; ++j)
        {
            constexpr float scale = 1.0f / 1.0f;
            const float offset = 20.0f;
            glm::mat4 translation = glm::translate(glm::vec3(i * offset, 0.0f, j * offset));
            auto node = std::make_unique<RenderNode>(*m_transformBuffer, m_transformBuffer->getNextIndex());
            node->transformPack->M = translation * glm::rotate(glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
                                     glm::scale(glm::vec3(scale));
            node->geometry = m_resourceContext->getGeometry("ocean");
            node->pass(MainPass).material = material;

            struct OceanPC
            {
                float t;
                uint32_t N;
            };

            OceanPC pc = {m_time, N};
            node->pass(MainPass).setPushConstants(pc);
            m_renderNodes.emplace_back(std::move(node));
        }
    }

    m_renderer->getDevice().flushDescriptorUpdates();
}

OceanScene::~OceanScene() {}

void OceanScene::resize(int width, int height)
{
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(width, height);
    m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);
}

void OceanScene::update(float dt)
{
    m_cameraController->update(dt);
    const CameraParameters cameraParams = m_cameraController->getCameraParameters();

    m_transformBuffer->update(cameraParams.V, cameraParams.P);
    m_skybox->updateTransforms(cameraParams.V, cameraParams.P);

    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(cameraParams);

    if (!paused)
    {
        m_time += dt;
    }

    OceanParams.time = m_time;
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
    if (ImGui::SliderFloat("Wind Speed X", &WindVelocity.x, 0.0f, 100.0f))
    {
        OceanParams.windSpeed = glm::length(WindVelocity);
        OceanParams.windDirection = WindVelocity / OceanParams.windSpeed;
        OceanParams.Lw = OceanParams.windSpeed * OceanParams.windSpeed / 9.81f;
    }
    if (ImGui::SliderFloat("Wind Speed Z", &WindVelocity.y, 0.0f, 100.0f))
    {
        OceanParams.windSpeed = glm::length(WindVelocity);
        OceanParams.windDirection = WindVelocity / OceanParams.windSpeed;
        OceanParams.Lw = OceanParams.windSpeed * OceanParams.windSpeed / 9.81f;
    }
    ImGui::SliderFloat("Amplitude", &OceanParams.A, 0.0f, 1000.0f);
    ImGui::SliderFloat("Small Waves", &OceanParams.smallWaves, 0.0f, 10.0f);
    ImGui::SliderFloat("Choppiness", &Choppiness, 0.0f, 1.0f);
    ImGui::End();
}

std::unique_ptr<VulkanImage> OceanScene::createInitialSpectrum()
{
    const auto oceanSpectrum{createOceanSpectrum(0, N, Lx)};

    auto image = createStorageImage(m_renderer->getDevice(), 1, N, N, VK_FORMAT_R32G32B32A32_SFLOAT);
    std::shared_ptr<VulkanBuffer> stagingBuffer = createStagingBuffer(m_renderer->getDevice(), oceanSpectrum);
    m_renderer->enqueueResourceUpdate(
        [img = image.get(), stagingBuffer](VkCommandBuffer cmdBuffer)
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
        bitReversePass.pipeline = createBitReversalPipeline(m_renderer, bitReversePass.workGroupSize);
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
            OscillationPass,
            image + "BitReversePass0",
            [this, image, imageLayerRead](
                const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
            {
                VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.image = m_resourceContext->imageCache.getImage(image).getHandle();
                barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseArrayLayer = imageLayerRead;
                barrier.subresourceRange.layerCount = 1;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                cmdBuffer.insertImageMemoryBarrier(
                    barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
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
        fftPass.pipeline = createIFFTPipeline(m_renderer, fftPass.workGroupSize, "ifft-hori.comp");
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
                    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.image = m_resourceContext->imageCache.getImage(image).getHandle();
                    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseArrayLayer = imageLayerRead;
                    barrier.subresourceRange.layerCount = 1;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;
                    cmdBuffer.insertImageMemoryBarrier(
                        barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
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
                    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.image = m_resourceContext->imageCache.getImage(image).getHandle();
                    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseArrayLayer = imageLayerRead;
                    barrier.subresourceRange.layerCount = 1;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;
                    cmdBuffer.insertImageMemoryBarrier(
                        barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
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
        bitReversePass2.pipeline = createBitReversalPipeline(m_renderer, bitReversePass2.workGroupSize);
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
                VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.image = m_resourceContext->imageCache.getImage(image).getHandle();
                barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseArrayLayer = imageLayerRead;
                barrier.subresourceRange.layerCount = 1;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                cmdBuffer.insertImageMemoryBarrier(
                    barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
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
        fftPass.pipeline = createIFFTPipeline(m_renderer, fftPass.workGroupSize, "ifft-vert.comp");
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
                GeometryPass,
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
                    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.image = m_resourceContext->imageCache.getImage(image).getHandle();
                    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseArrayLayer = imageLayerRead;
                    barrier.subresourceRange.layerCount = 1;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;
                    cmdBuffer.insertImageMemoryBarrier(
                        barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
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
                    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.image = m_resourceContext->imageCache.getImage(image).getHandle();
                    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseArrayLayer = imageLayerRead;
                    barrier.subresourceRange.layerCount = 1;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;
                    cmdBuffer.insertImageMemoryBarrier(
                        barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                });
        }

        std::swap(imageLayerRead, imageLayerWrite);
    }

    return imageLayerRead;
}

} // namespace crisp