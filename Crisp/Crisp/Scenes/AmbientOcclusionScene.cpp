#include <Crisp/Scenes/AmbientOcclusionScene.hpp>

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Image/Io/Utils.hpp>

#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/RenderPasses/BlurPass.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>

#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>
#include <Crisp/vulkan/VulkanSampler.hpp>

#include <Crisp/Models/Skybox.hpp>

#include <Crisp/GUI/Form.hpp>
#include <Crisp/GUI/Label.hpp>
#include <Crisp/GUI/Slider.hpp>

#include <Crisp/Math/Warp.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>

#include <random>

namespace crisp
{
namespace
{
glm::vec4 pc(0.2f, 0.0f, 0.0f, 1.0f);

struct BlurParams
{
    float dirX;
    float dirY;
    float sigma;
    int radius;
};

BlurParams blurH = {0.0f, 1.0f / Application::DefaultWindowWidth, 1.0f, 3};
BlurParams blurV = {1.0f / Application::DefaultWindowHeight, 0.0f, 1.0f, 3};

std::unique_ptr<VulkanRenderPass> createAmbientOcclusionPass(Renderer& renderer, RenderTargetCache& renderTargetCache)
{
    std::vector<RenderTarget*> renderTargets(1);
    renderTargets[0] = renderTargetCache.addRenderTarget(
        "AmbientOcclusionMap",
        RenderTargetBuilder()
            .setFormat(VK_FORMAT_R32G32B32A32_SFLOAT)
            .setBuffered(true)
            .configureColorRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT)
            .setSize(renderer.getSwapChainExtent(), true)
            .create(renderer.getDevice()));

    return RenderPassBuilder()
        .setAttachmentCount(1)
        .setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .addDependency(
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(renderer.getDevice(), renderer.getSwapChainExtent(), std::move(renderTargets));
}

} // namespace

AmbientOcclusionScene::AmbientOcclusionScene(Renderer* renderer, Application* app)
    : AbstractScene(app, renderer)
    , m_ssaoParams{128, 0.5f}
{
    m_cameraController = std::make_unique<FreeCameraController>(m_app->getWindow());
    /*m_cameraController->getCamera().setRotation(glm::pi<float>() * 0.5f, 0.0f, 0.0f);
    m_cameraController->getCamera().setPosition(glm::vec3(0.0f, 2.0f, 1.0f));*/
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

    std::default_random_engine randomEngine(42);
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    glm::vec4 samples[512];
    for (int i = 0; i < 512; i++)
    {
        float x = distribution(randomEngine);
        float y = distribution(randomEngine);
        float r = distribution(randomEngine);
        glm::vec3 s = std::sqrt(r) * warp::squareToUniformHemisphere(glm::vec2(x, y));
        samples[i] = glm::vec4(s, 1.0f);
    }

    std::vector<glm::vec4> noiseTexData;
    for (int i = 0; i < 16; i++)
    {
        glm::vec3 s(distribution(randomEngine) * 2.0f - 1.0f, distribution(randomEngine) * 2.0f - 1.0f, 0.0f);
        s = glm::normalize(s);
        noiseTexData.push_back(glm::vec4(s, 1.0f));
    }

    VkImageCreateInfo noiseTexInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    noiseTexInfo.flags = 0;
    noiseTexInfo.imageType = VK_IMAGE_TYPE_2D;
    noiseTexInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    noiseTexInfo.extent = {4, 4, 1u};
    noiseTexInfo.mipLevels = 1;
    noiseTexInfo.arrayLayers = 1;
    noiseTexInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    noiseTexInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    noiseTexInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    noiseTexInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    noiseTexInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addImageWithView(
        "noise",
        createVulkanImage(*m_renderer, noiseTexData.size() * sizeof(glm::vec4), noiseTexData.data(), noiseTexInfo));

    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice()));
    imageCache.addSampler("nearestClamp", createNearestClampSampler(m_renderer->getDevice()));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice()));

    m_resourceContext->createUniformBuffer("samples", sizeof(samples), BufferUpdatePolicy::Constant, samples);

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 2);

    auto mainPass = createForwardLightingPass(
        m_renderer->getDevice(), m_resourceContext->renderTargetCache, renderer->getSwapChainExtent());
    auto ssaoPass = createAmbientOcclusionPass(*m_renderer, m_resourceContext->renderTargetCache);
    auto blurHPass = createBlurPass(
        m_renderer->getDevice(),
        m_resourceContext->renderTargetCache,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        m_renderer->getSwapChainExtent(),
        true,
        "BlurHMap");
    auto blurVPass = createBlurPass(
        m_renderer->getDevice(),
        m_resourceContext->renderTargetCache,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        m_renderer->getSwapChainExtent(),
        true,
        "BlurVMap");

    VulkanPipeline* colorPipeline = m_resourceContext->createPipeline("color", "UniformColor.lua", *mainPass, 0);
    Material* colorMaterial = m_resourceContext->createMaterial("color", colorPipeline);
    colorMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());

    VulkanPipeline* normalPipeline = m_resourceContext->createPipeline("normal", "DepthNormal.lua", *mainPass, 0);
    Material* normalMaterial = m_resourceContext->createMaterial("normal", normalPipeline);
    normalMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());

    const VertexLayoutDescription positionFormat = {{VertexAttribute::Position}};
    m_resourceContext->addGeometry(
        "floorPos", std::make_unique<Geometry>(*m_renderer, createPlaneMesh(flatten(positionFormat)), positionFormat));

    const VertexLayoutDescription vertexFormat = {
        {VertexAttribute::Position, VertexAttribute::Normal}
    };
    m_resourceContext->addGeometry(
        "sponza",
        std::make_unique<Geometry>(
            *m_renderer,
            loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/Sponza-master/sponza.obj", flatten(vertexFormat))
                .unwrap(),
            vertexFormat));

    m_skybox = std::make_unique<Skybox>(m_renderer, *mainPass, "Creek");

    m_renderGraph->addRenderPass("mainPass", std::move(mainPass));
    m_renderGraph->addRenderPass("ssaoPass", std::move(ssaoPass));
    m_renderGraph->addRenderPass("blurHPass", std::move(blurHPass));
    m_renderGraph->addRenderPass("blurVPass", std::move(blurVPass));
    m_renderGraph->addDependency("mainPass", "ssaoPass", 0);
    m_renderGraph->addDependency("ssaoPass", "blurHPass", 0);
    m_renderGraph->addDependency("blurHPass", "blurVPass", 0);
    m_renderGraph->addDependency("blurVPass", "SCREEN", 0);
    m_renderGraph->sortRenderPasses().unwrap();

    m_renderer->setSceneImageView(m_renderGraph->getNode("blurVPass").renderPass.get(), 0);

    m_floorNode = std::make_unique<RenderNode>(*m_transformBuffer, m_transformBuffer->getNextIndex());
    m_floorNode->transformPack->M =
        glm::translate(glm::vec3(0.0f, -1.0f, 0.0f)) * glm::scale(glm::vec3(50.0f, 1.0f, 50.0f));
    m_floorNode->geometry = m_resourceContext->getGeometry("floorPos");
    m_floorNode->pass("mainPass").material = colorMaterial;
    m_floorNode->pass("mainPass").pipeline = colorPipeline;
    m_floorNode->pass("mainPass").setPushConstantView(pc);

    m_sponzaNode = std::make_unique<RenderNode>(*m_transformBuffer, m_transformBuffer->getNextIndex());
    m_sponzaNode->transformPack->M = glm::mat4(1.0f);
    m_sponzaNode->geometry = m_resourceContext->getGeometry("sponza");
    m_sponzaNode->pass("mainPass").material = normalMaterial;
    m_sponzaNode->pass("mainPass").pipeline = normalPipeline;
    m_sponzaNode->pass("mainPass").setPushConstantView(pc);

    // mainPassNode.renderNodes.push_back(m_skybox->createRenderNode());

    auto ssaoNode = m_resourceContext->createPostProcessingEffectNode(
        "ssao", "Ssao.lua", m_renderGraph->getRenderPass("ssaoPass"), "ssaoPass");
    ssaoNode->pass("ssaoPass")
        .material->writeDescriptor(
            0, 0, m_renderGraph->getRenderPass("mainPass"), 0, &imageCache.getSampler("nearestClamp"));
    ssaoNode->pass("ssaoPass").material->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));
    ssaoNode->pass("ssaoPass").material->writeDescriptor(0, 2, *m_resourceContext->getUniformBuffer("samples"));
    ssaoNode->pass("ssaoPass")
        .material->writeDescriptor(0, 3, imageCache.getImageView("noise"), imageCache.getSampler("linearRepeat"));
    ssaoNode->pass("ssaoPass").setPushConstantView(m_ssaoParams);

    auto blurHNode = m_resourceContext->createPostProcessingEffectNode(
        "blurH", "GaussianBlur.lua", m_renderGraph->getRenderPass("blurHPass"), "blurHPass");
    blurHNode->pass("blurHPass")
        .material->writeDescriptor(
            0, 0, m_renderGraph->getRenderPass("ssaoPass"), 0, &imageCache.getSampler("linearClamp"));
    blurHNode->pass("blurHPass").setPushConstantView(blurH);

    auto blurVNode = m_resourceContext->createPostProcessingEffectNode(
        "blurV", "GaussianBlur.lua", m_renderGraph->getRenderPass("blurVPass"), "blurVPass");
    blurVNode->pass("blurVPass")
        .material->writeDescriptor(
            0, 0, m_renderGraph->getRenderPass("blurHPass"), 0, &imageCache.getSampler("linearClamp"));
    blurVNode->pass("blurVPass").setPushConstantView(blurV);

    m_renderer->getDevice().flushDescriptorUpdates();

    createGui();
}

AmbientOcclusionScene::~AmbientOcclusionScene()
{
    m_app->getForm()->remove("ambientOcclusionPanel");
}

void AmbientOcclusionScene::resize(int width, int height)
{
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(width, height);
    m_renderer->setSceneImageView(m_renderGraph->getNode("blurVPass").renderPass.get(), 0);

    auto* mainPass = m_renderGraph->getNode("mainPass").renderPass.get();
    m_resourceContext->getMaterial("Ssao.lua")
        ->writeDescriptor(0, 0, *mainPass, 0, &m_resourceContext->imageCache.getSampler("nearestClamp"));
}

void AmbientOcclusionScene::update(float dt)
{
    m_cameraController->update(dt);
    const CameraParameters cameraParams = m_cameraController->getCameraParameters();
    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(cameraParams);
    auto pos = m_cameraController->getCamera().getPosition();

    m_transformBuffer->update(cameraParams.V, cameraParams.P);
    m_skybox->updateTransforms(cameraParams.V, cameraParams.P);
}

void AmbientOcclusionScene::render()
{
    m_renderGraph->clearCommandLists();
    m_renderGraph->buildCommandLists(m_resourceContext->getRenderNodes());
    m_renderGraph->addToCommandLists(*m_floorNode);
    m_renderGraph->addToCommandLists(*m_sponzaNode);
    m_renderGraph->executeCommandLists();
}

void AmbientOcclusionScene::setNumSamples(int numSamples)
{
    m_ssaoParams.numSamples = numSamples;
}

void AmbientOcclusionScene::setRadius(double radius)
{
    m_ssaoParams.radius = static_cast<float>(radius);
}

void AmbientOcclusionScene::createGui()
{
    using namespace gui;
    auto panel = std::make_unique<Panel>(m_app->getForm());
    panel->setId("ambientOcclusionPanel");
    panel->setPadding({20, 20});
    panel->setPosition({20, 40});
    panel->setSizingPolicy(SizingPolicy::WrapContent, SizingPolicy::WrapContent);

    int y = 0;

    auto numSamplesLabel = std::make_unique<Label>(m_app->getForm(), "Number of samples");
    numSamplesLabel->setPosition({0, y});
    panel->addControl(std::move(numSamplesLabel));
    y += 20;

    auto numSamplesSlider = std::make_unique<IntSlider>(m_app->getForm());
    numSamplesSlider->setId("numSamplesSlider");
    numSamplesSlider->setPosition({0, y});
    numSamplesSlider->setDiscreteValues({16, 32, 64, 128, 256, 512});
    numSamplesSlider->setIndexValue(3);
    numSamplesSlider->valueChanged.subscribe<&AmbientOcclusionScene::setNumSamples>(this);
    panel->addControl(std::move(numSamplesSlider));
    y += 30;

    auto radiusLabel = std::make_unique<Label>(m_app->getForm(), "Radius");
    radiusLabel->setPosition({0, y});
    panel->addControl(std::move(radiusLabel));
    y += 20;

    auto radiusSlider = std::make_unique<DoubleSlider>(m_app->getForm());
    radiusSlider->setId("radiusSlider");
    radiusSlider->setPosition({0, y});
    radiusSlider->setMinValue(0.1);
    radiusSlider->setMaxValue(2.0);
    radiusSlider->setIncrement(0.1);
    radiusSlider->setPrecision(1);
    radiusSlider->setValue(0.5);
    radiusSlider->valueChanged.subscribe<&AmbientOcclusionScene::setRadius>(this);
    panel->addControl(std::move(radiusSlider));
    y += 30;

    m_app->getForm()->add(std::move(panel));
}
} // namespace crisp