#include "AmbientOcclusionScene.hpp"

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>
#include <CrispCore/IO/ImageLoader.hpp>

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
#include <CrispCore/Mesh/TriangleMeshUtils.hpp>

#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>
#include <Crisp/vulkan/VulkanSampler.hpp>

#include <Crisp/Models/Skybox.hpp>

#include <Crisp/GUI/Form.hpp>
#include <Crisp/GUI/Label.hpp>
#include <Crisp/GUI/Slider.hpp>

#include <CrispCore/IO/MeshLoader.hpp>
#include <CrispCore/Math/Warp.hpp>

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

BlurParams blurH = { 0.0f, 1.0f / Application::DefaultWindowWidth, 1.0f, 3 };
BlurParams blurV = { 1.0f / Application::DefaultWindowHeight, 0.0f, 1.0f, 3 };

std::unique_ptr<VulkanRenderPass> createAmbientOcclusionPass(Renderer& renderer)
{
    auto [handle, attachmentDescriptions] =
        RenderPassBuilder()
            .addAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(renderer.getDevice().getHandle());

    RenderPassDescription description{};
    description.isSwapChainDependent = true;
    description.subpassCount = 1;
    description.attachmentDescriptions = std::move(attachmentDescriptions);
    description.renderTargetInfos.resize(description.attachmentDescriptions.size());
    description.renderTargetInfos[0].configureColorRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT);

    return std::make_unique<VulkanRenderPass>(renderer, handle, std::move(description));
}

} // namespace

AmbientOcclusionScene::AmbientOcclusionScene(Renderer* renderer, Application* app)
    : m_renderer(renderer)
    , m_app(app)
    , m_resourceContext(std::make_unique<ResourceContext>(renderer))
    , m_ssaoParams{ 128, 0.5f }
{
    m_cameraController = std::make_unique<FreeCameraController>(m_app->getWindow());
    /*m_cameraController->getCamera().setRotation(glm::pi<float>() * 0.5f, 0.0f, 0.0f);
    m_cameraController->getCamera().setPosition(glm::vec3(0.0f, 2.0f, 1.0f));*/
    m_cameraBuffer =
        std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

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

    VkImageCreateInfo noiseTexInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    noiseTexInfo.flags = 0;
    noiseTexInfo.imageType = VK_IMAGE_TYPE_2D;
    noiseTexInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    noiseTexInfo.extent = { 4, 4, 1u };
    noiseTexInfo.mipLevels = 1;
    noiseTexInfo.arrayLayers = 1;
    noiseTexInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    noiseTexInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    noiseTexInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    noiseTexInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    noiseTexInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_resourceContext->addImageWithView("noise", createTexture(m_renderer, noiseTexData.size() * sizeof(glm::vec4),
                                                     noiseTexData.data(), noiseTexInfo, VK_IMAGE_ASPECT_COLOR_BIT));

    m_resourceContext->addSampler("linearRepeat",
        std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT));
    m_resourceContext->addSampler("nearestClamp",
        std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_NEAREST, VK_FILTER_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    m_resourceContext->addSampler("linearClamp",
        std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));

    m_sampleBuffer =
        std::make_unique<UniformBuffer>(m_renderer, sizeof(samples), BufferUpdatePolicy::Constant, samples);

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 2);

    auto mainPass = std::make_unique<ForwardLightingPass>(*m_renderer);
    auto ssaoPass = createAmbientOcclusionPass(*m_renderer);
    auto blurHPass = createBlurPass(*m_renderer, VK_FORMAT_R32G32B32A32_SFLOAT);
    auto blurVPass = createBlurPass(*m_renderer, VK_FORMAT_R32G32B32A32_SFLOAT);

    VulkanPipeline* colorPipeline = m_resourceContext->createPipeline("color", "UniformColor.lua", *mainPass, 0);
    Material* colorMaterial = m_resourceContext->createMaterial("color", colorPipeline);
    colorMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());

    VulkanPipeline* normalPipeline = m_resourceContext->createPipeline("normal", "DepthNormal.lua", *mainPass, 0);
    Material* normalMaterial = m_resourceContext->createMaterial("normal", normalPipeline);
    normalMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());

    std::vector<VertexAttributeDescriptor> shadowFormat = { VertexAttribute::Position };
    m_resourceContext->addGeometry("floorPos", std::make_unique<Geometry>(m_renderer, createPlaneMesh(shadowFormat)));

    std::vector<VertexAttributeDescriptor> vertexFormat = { VertexAttribute::Position, VertexAttribute::Normal };
    m_resourceContext->addGeometry("sponza",
        std::make_unique<Geometry>(m_renderer,
            loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/Sponza-master/sponza.obj", vertexFormat)));

    m_skybox = std::make_unique<Skybox>(m_renderer, *mainPass, "Creek");

    m_renderGraph = std::make_unique<RenderGraph>(m_renderer);
    m_renderGraph->addRenderPass("mainPass", std::move(mainPass));
    m_renderGraph->addRenderPass("ssaoPass", std::move(ssaoPass));
    m_renderGraph->addRenderPass("blurHPass", std::move(blurHPass));
    m_renderGraph->addRenderPass("blurVPass", std::move(blurVPass));
    m_renderGraph->addRenderTargetLayoutTransition("mainPass", "ssaoPass", 0);
    m_renderGraph->addRenderTargetLayoutTransition("ssaoPass", "blurHPass", 0);
    m_renderGraph->addRenderTargetLayoutTransition("blurHPass", "blurVPass", 0);
    m_renderGraph->addRenderTargetLayoutTransition("blurVPass", "SCREEN", 0);
    m_renderGraph->sortRenderPasses().unwrap();

    m_renderer->setSceneImageView(m_renderGraph->getNode("blurVPass").renderPass.get(), 0);

    m_floorNode = std::make_unique<RenderNode>(*m_transformBuffer, 0);
    m_floorNode->transformPack->M =
        glm::translate(glm::vec3(0.0f, -1.0f, 0.0f)) * glm::scale(glm::vec3(50.0f, 1.0f, 50.0f));
    m_floorNode->geometry = m_resourceContext->getGeometry("floorPos");
    m_floorNode->pass("mainPass").material = colorMaterial;
    m_floorNode->pass("mainPass").pipeline = colorPipeline;
    m_floorNode->pass("mainPass").setPushConstantView(pc);

    m_sponzaNode = std::make_unique<RenderNode>(*m_transformBuffer, 1);
    m_sponzaNode->transformPack->M = glm::mat4(1.0f);
    m_sponzaNode->geometry = m_resourceContext->getGeometry("sponza");
    m_sponzaNode->pass("mainPass").material = normalMaterial;
    m_sponzaNode->pass("mainPass").pipeline = normalPipeline;
    m_sponzaNode->pass("mainPass").setPushConstantView(pc);

    // mainPassNode.renderNodes.push_back(m_skybox->createRenderNode());

    auto ssaoNode = m_resourceContext->createPostProcessingEffectNode("ssao", "Ssao.lua",
        m_renderGraph->getRenderPass("ssaoPass"), "ssaoPass");
    ssaoNode->pass("ssaoPass")
        .material->writeDescriptor(0, 0, m_renderGraph->getRenderPass("mainPass"), 0,
            m_resourceContext->getSampler("nearestClamp"));
    ssaoNode->pass("ssaoPass").material->writeDescriptor(0, 1, *m_cameraBuffer);
    ssaoNode->pass("ssaoPass").material->writeDescriptor(0, 2, *m_sampleBuffer);
    ssaoNode->pass("ssaoPass")
        .material->writeDescriptor(0, 3, *m_resourceContext->getImageView("noise"),
            m_resourceContext->getSampler("linearRepeat"));
    ssaoNode->pass("ssaoPass").setPushConstantView(m_ssaoParams);

    auto blurHNode = m_resourceContext->createPostProcessingEffectNode("blurH", "GaussianBlur.lua",
        m_renderGraph->getRenderPass("blurHPass"), "blurHPass");
    blurHNode->pass("blurHPass")
        .material->writeDescriptor(0, 0, m_renderGraph->getRenderPass("ssaoPass"), 0,
            m_resourceContext->getSampler("linearClamp"));
    blurHNode->pass("blurHPass").setPushConstantView(blurH);

    auto blurVNode = m_resourceContext->createPostProcessingEffectNode("blurV", "GaussianBlur.lua",
        m_renderGraph->getRenderPass("blurVPass"), "blurVPass");
    blurVNode->pass("blurVPass")
        .material->writeDescriptor(0, 0, m_renderGraph->getRenderPass("blurHPass"), 0,
            m_resourceContext->getSampler("linearClamp"));
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
        ->writeDescriptor(0, 0, *mainPass, 0, m_resourceContext->getSampler("nearestClamp"));
}

void AmbientOcclusionScene::update(float dt)
{
    m_cameraController->update(dt);
    const CameraParameters cameraParams = m_cameraController->getCameraParameters();
    m_cameraBuffer->updateStagingBuffer(cameraParams);
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
    panel->setPadding({ 20, 20 });
    panel->setPosition({ 20, 40 });
    panel->setSizingPolicy(SizingPolicy::WrapContent, SizingPolicy::WrapContent);

    int y = 0;

    auto numSamplesLabel = std::make_unique<Label>(m_app->getForm(), "Number of samples");
    numSamplesLabel->setPosition({ 0, y });
    panel->addControl(std::move(numSamplesLabel));
    y += 20;

    auto numSamplesSlider = std::make_unique<IntSlider>(m_app->getForm());
    numSamplesSlider->setId("numSamplesSlider");
    numSamplesSlider->setPosition({ 0, y });
    numSamplesSlider->setDiscreteValues({ 16, 32, 64, 128, 256, 512 });
    numSamplesSlider->setIndexValue(3);
    numSamplesSlider->valueChanged.subscribe<&AmbientOcclusionScene::setNumSamples>(this);
    panel->addControl(std::move(numSamplesSlider));
    y += 30;

    auto radiusLabel = std::make_unique<Label>(m_app->getForm(), "Radius");
    radiusLabel->setPosition({ 0, y });
    panel->addControl(std::move(radiusLabel));
    y += 20;

    auto radiusSlider = std::make_unique<DoubleSlider>(m_app->getForm());
    radiusSlider->setId("radiusSlider");
    radiusSlider->setPosition({ 0, y });
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