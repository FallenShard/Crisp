#include "AmbientOcclusionScene.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "IO/ImageFileBuffer.hpp"
#include "Camera/CameraController.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph.hpp"
#include "Renderer/Material.hpp"

#include "Renderer/RenderPasses/ForwardLightingPass.hpp"
#include "Renderer/RenderPasses/AmbientOcclusionPass.hpp"
#include "Renderer/RenderPasses/BlurPass.hpp"
#include "Renderer/Pipelines/UniformColorPipeline.hpp"
#include "Renderer/Pipelines/NormalPipeline.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/VulkanImageUtils.hpp"

#include "Geometry/Geometry.hpp"
#include "Geometry/MeshGenerators.hpp"

#include "vulkan/VulkanSampler.hpp"
#include "vulkan/VulkanImage.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanDevice.hpp"

#include "Models/Skybox.hpp"

#include "GUI/Form.hpp"
#include "GUI/Label.hpp"
#include "GUI/Slider.hpp"

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

        BlurParams blurH = { 0.0f, 1.0f / Application::DefaultWindowWidth,  1.0f, 3 };
        BlurParams blurV = { 1.0f / Application::DefaultWindowHeight, 0.0f, 1.0f, 3 };
    }

    AmbientOcclusionScene::AmbientOcclusionScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_ssaoParams{ 128, 0.5f }
    {
        m_cameraController = std::make_unique<CameraController>(m_app->getWindow());
        m_cameraController->getCamera().setRotation(glm::pi<float>() * 0.5f, 0.0f, 0.0f);
        m_cameraController->getCamera().setPosition(glm::vec3(0.0f, 2.0f, 1.0f));
        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        std::default_random_engine randomEngine(std::random_device{}());
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
        noiseTexInfo.flags         = 0;
        noiseTexInfo.imageType     = VK_IMAGE_TYPE_2D;
        noiseTexInfo.format        = VK_FORMAT_R32G32B32A32_SFLOAT;
        noiseTexInfo.extent        = { 4, 4, 1u };
        noiseTexInfo.mipLevels     = 1;
        noiseTexInfo.arrayLayers   = 1;
        noiseTexInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        noiseTexInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        noiseTexInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        noiseTexInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        noiseTexInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        auto noiseTex = createTexture(m_renderer, noiseTexData.size() * sizeof(glm::vec4), noiseTexData.data(), noiseTexInfo, VK_IMAGE_ASPECT_COLOR_BIT);
        noiseTex->setTag("noiseTex");
        auto noiseTexView = noiseTex->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1);

        m_noiseSampler       = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_REPEAT);
        m_nearestSampler     = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        m_linearClampSampler = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_sampleBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(samples), BufferUpdatePolicy::Constant, samples);

        m_transforms.resize(2);
        m_transformBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        auto mainPass = std::make_unique<ForwardLightingPass>(m_renderer);
        auto ssaoPass = std::make_unique<AmbientOcclusionPass>(m_renderer);
        auto blurHPass = std::make_unique<BlurPass>(m_renderer, VK_FORMAT_R32G32B32A32_SFLOAT, m_renderer->getSwapChainExtent());
        auto blurVPass = std::make_unique<BlurPass>(m_renderer, VK_FORMAT_R32G32B32A32_SFLOAT, m_renderer->getSwapChainExtent());



        auto colorPipeline = createColorPipeline(m_renderer, mainPass.get());
        auto colorMaterial = std::make_unique<Material>(colorPipeline.get());
        colorMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));

        auto normalPipeline = createNormalPipeline(m_renderer, mainPass.get());





        std::vector<VertexAttributeDescriptor> shadowFormat = { VertexAttribute::Position };
        auto planeGeometry = std::make_unique<Geometry>(m_renderer, createPlaneMesh(shadowFormat));

        std::vector<VertexAttributeDescriptor> vertexFormat = { VertexAttribute::Position, VertexAttribute::Normal };
        auto sponzaGeometry = std::make_unique<Geometry>(m_renderer, m_renderer->getResourcesPath() / "Meshes/Sponza-master/sponza.obj", vertexFormat);

        m_skybox = std::make_unique<Skybox>(m_renderer, *mainPass, "Creek");

        m_images.push_back(std::move(noiseTex));
        m_imageViews.push_back(std::move(noiseTexView));

        m_geometries.push_back(std::move(planeGeometry));
        m_geometries.push_back(std::move(sponzaGeometry));

        m_pipelines.push_back(std::move(colorPipeline));
        m_pipelines.push_back(std::move(normalPipeline));

        m_materials.push_back(std::move(colorMaterial));

        m_renderGraph = std::make_unique<RenderGraph>(m_renderer);
        m_renderGraph->addRenderPass("mainPass", std::move(mainPass));
        m_renderGraph->addRenderPass("ssaoPass", std::move(ssaoPass));
        m_renderGraph->addRenderPass("blurHPass", std::move(blurHPass));
        m_renderGraph->addRenderPass("blurVPass", std::move(blurVPass));
        m_renderGraph->addRenderTargetLayoutTransition("mainPass", "ssaoPass", 0);
        m_renderGraph->addRenderTargetLayoutTransition("ssaoPass", "blurHPass", 0);
        m_renderGraph->addRenderTargetLayoutTransition("blurHPass", "blurVPass", 0);
        m_renderGraph->addRenderTargetLayoutTransition("blurVPass", "SCREEN", 0);
        m_renderGraph->sortRenderPasses();

        m_renderer->setSceneImageView(m_renderGraph->getNode("blurVPass").renderPass.get(), 0);

        m_floorNode = std::make_unique<RenderNode>(m_transformBuffer.get(), &m_transforms[0], 0);
        m_floorNode->transformPack->M = glm::translate(glm::vec3(0.0f, -1.0f, 0.0f)) * glm::scale(glm::vec3(50.0f, 1.0f, 50.0f));
        m_floorNode->geometry = m_geometries[0].get();
        m_floorNode->pass("mainPass").material = m_materials[0].get();
        m_floorNode->pass("mainPass").setPushConstantView(pc);

        m_sponzaNode = std::make_unique<RenderNode>(m_transformBuffer.get(), &m_transforms[1], 1);
        m_sponzaNode->transformPack->M = glm::mat4(1.0f);
        m_sponzaNode->geometry = m_geometries[1].get();
        m_sponzaNode->pass("mainPass").material = m_materials[0].get();
        m_sponzaNode->pass("mainPass").pipeline = m_pipelines[1].get();
        m_sponzaNode->pass("mainPass").setPushConstantView(pc);

        //mainPassNode.renderNodes.push_back(m_skybox->createRenderNode());

        m_ssaoNode = createPostProcessingEffect("ssaoPass", "Ssao.lua");
        m_ssaoNode->pass("ssaoPass").material->writeDescriptor(0, 0, *m_renderGraph->getNode("mainPass").renderPass, 0, m_nearestSampler.get());
        m_ssaoNode->pass("ssaoPass").material->writeDescriptor(0, 1, *m_cameraBuffer);
        m_ssaoNode->pass("ssaoPass").material->writeDescriptor(0, 2, *m_sampleBuffer);
        m_ssaoNode->pass("ssaoPass").material->writeDescriptor(0, 3, *m_imageViews.back(), m_noiseSampler.get());
        m_ssaoNode->pass("ssaoPass").setPushConstantView(m_ssaoParams);

        m_blurHNode = createPostProcessingEffect("blurHPass", "GaussianBlur.lua");
        m_blurHNode->pass("blurHPass").material->writeDescriptor(0, 0, *m_renderGraph->getNode("ssaoPass").renderPass,  0, m_linearClampSampler.get());
        m_blurHNode->pass("blurHPass").setPushConstantView(blurH);

        m_blurVNode = createPostProcessingEffect("blurVPass", "GaussianBlur.lua");
        m_blurVNode->pass("blurVPass").material->writeDescriptor(0, 0, *m_renderGraph->getNode("blurHPass").renderPass, 0, m_linearClampSampler.get());
        m_blurVNode->pass("blurVPass").setPushConstantView(blurV);

        m_renderer->getDevice()->flushDescriptorUpdates();

        createGui();
    }

    AmbientOcclusionScene::~AmbientOcclusionScene()
    {
        m_app->getForm()->remove("ambientOcclusionPanel");
    }

    void AmbientOcclusionScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode("ssaoPass").renderPass.get(), 0);

        auto* mainPass = m_renderGraph->getNode("mainPass").renderPass.get();
        m_materials[1]->writeDescriptor(0, 0, *mainPass, 0, m_nearestSampler.get());
    }

    void AmbientOcclusionScene::update(float dt)
    {
        m_cameraController->update(dt);
        m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));
        auto pos = m_cameraController->getCamera().getPosition();

        const auto& V = m_cameraController->getViewMatrix();
        const auto& P = m_cameraController->getProjectionMatrix();

        for (auto& trans : m_transforms)
        {
            trans.MV  = V * trans.M;
            trans.MVP = P * trans.MV;
        }
        m_transformBuffer->updateStagingBuffer(m_transforms.data(), m_transforms.size() * sizeof(TransformPack));

        m_skybox->updateTransforms(V, P);
    }

    void AmbientOcclusionScene::render()
    {
        m_renderGraph->clearCommandLists();
        m_renderGraph->addToCommandLists(*m_floorNode);
        m_renderGraph->addToCommandLists(*m_sponzaNode);
        m_renderGraph->addToCommandLists(*m_ssaoNode);
        m_renderGraph->addToCommandLists(*m_blurHNode);
        m_renderGraph->addToCommandLists(*m_blurVNode);
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

    std::unique_ptr<RenderNode> AmbientOcclusionScene::createPostProcessingEffect(std::string renderPassName, std::string pipelineLuaFile)
    {
        auto& pass = m_renderGraph->getNode(renderPassName).renderPass;
        m_pipelines.push_back(m_renderer->createPipelineFromLua(pipelineLuaFile, *pass, 0));
        m_materials.push_back(std::make_unique<Material>(m_pipelines.back().get()));

        auto renderNode = std::make_unique<RenderNode>();
        renderNode->geometry = m_renderer->getFullScreenGeometry();
        renderNode->pass(renderPassName).material = m_materials.back().get();
        renderNode->pass(renderPassName).setPushConstantView(m_ssaoParams);
        return renderNode;
    }

    void AmbientOcclusionScene::createGui()
    {
        using namespace gui;
        auto panel = std::make_unique<Panel>(m_app->getForm());
        panel->setId("ambientOcclusionPanel");
        panel->setPadding({ 20, 20 });
        panel->setPosition({ 20, 40 });
        panel->setVerticalSizingPolicy(SizingPolicy::WrapContent);
        panel->setHorizontalSizingPolicy(SizingPolicy::WrapContent);

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
}