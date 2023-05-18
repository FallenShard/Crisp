#include <Crisp/Scenes/ClusteredLightingScene.hpp>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>

#include <Crisp/Camera/FreeCameraController.hpp>

#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>
#include <Crisp/Vulkan/VulkanSampler.hpp>

#include <Crisp/Image/Io/Utils.hpp>

#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/Renderer/VulkanBufferUtils.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Models/Skybox.hpp>

#include <Crisp/Lights/DirectionalLight.hpp>
#include <Crisp/Lights/EnvironmentLighting.hpp>
#include <Crisp/Lights/LightSystem.hpp>

#include <Crisp/GUI/Button.hpp>
#include <Crisp/GUI/CheckBox.hpp>
#include <Crisp/GUI/ComboBox.hpp>
#include <Crisp/GUI/Form.hpp>
#include <Crisp/GUI/Label.hpp>
#include <Crisp/GUI/Slider.hpp>

#include <Crisp/Math/Constants.hpp>
#include <Crisp/Utils/LuaConfig.hpp>
#include <Crisp/Utils/Profiler.hpp>

namespace crisp
{
namespace
{
static constexpr uint32_t ShadowMapSize = 2048;
static constexpr uint32_t CascadeCount = 4;

static constexpr const char* MainPass = "mainPass";
static constexpr const char* CsmPass = "csmPass";
} // namespace

ClusteredLightingScene::ClusteredLightingScene(Renderer* renderer, Application* app)
    : AbstractScene(app, renderer)
{
    setupInput();

    // Camera
    m_cameraController = std::make_unique<FreeCameraController>(app->getWindow());
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

    m_renderGraph = std::make_unique<RenderGraph>(m_renderer);

    // Main render pass
    /*m_renderGraph->addRenderPass(MainPass, std::make_unique<ForwardLightingPass>(m_renderer->getDevice(),
                                               renderer->getSwapChainExtent(), VK_SAMPLE_COUNT_8_BIT));*/
    m_renderGraph->addRenderPass(
        MainPass,
        createForwardLightingPass(
            m_renderer->getDevice(), m_resourceContext->renderTargetCache, renderer->getSwapChainExtent()));

    // Wrap-up render graph definition
    m_renderGraph->addDependency(MainPass, "SCREEN", 2);

    m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 2);

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)),
        ShadowMapSize,
        CascadeCount);
    m_lightSystem->createPointLightBuffer(createRandomPointLights(1024));
    m_lightSystem->createTileGridBuffers(m_cameraController->getCameraParameters());
    m_lightSystem->addLightClusteringPass(*m_renderGraph, *m_resourceContext->getUniformBuffer("camera"));

    m_renderGraph->sortRenderPasses().unwrap();

    // Object transforms
    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 100);

    // Geometry setup
    const VertexLayoutDescription shadowVertexFormat = {{VertexAttribute::Position}};
    const VertexLayoutDescription cubeFormat{
        {VertexAttribute::Position, VertexAttribute::Normal}
    };
    m_resourceContext->addGeometry(
        "cubeRT", std::make_unique<Geometry>(*m_renderer, createCubeMesh(flatten(cubeFormat)), cubeFormat));
    m_resourceContext->addGeometry(
        "cubeShadow",
        std::make_unique<Geometry>(*m_renderer, createCubeMesh(flatten(shadowVertexFormat)), shadowVertexFormat));
    m_resourceContext->addGeometry(
        "sphereShadow",
        std::make_unique<Geometry>(*m_renderer, createSphereMesh(flatten(shadowVertexFormat)), shadowVertexFormat));
    m_resourceContext->addGeometry(
        "floorShadow",
        std::make_unique<Geometry>(*m_renderer, createPlaneMesh(flatten(shadowVertexFormat)), shadowVertexFormat));

    createCommonTextures();

    // for (uint32_t i = 0; i < CascadeCount; ++i)
    //{
    //     std::string key = "cascadedShadowMap" + std::to_string(i);
    //     auto csmPipeline = m_resourceContext->createPipeline(key, "ShadowMap.lua",
    //     m_renderGraph->getRenderPass(CsmPass), i); auto csmMaterial = m_resourceContext->createMaterial(key,
    //     csmPipeline); csmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    //     csmMaterial->writeDescriptor(0, 1, *m_lightSystem->getCascadedDirectionalLightBuffer(i));
    // }

    createPlane();
    createShaderball();

    m_renderer->getDevice().flushDescriptorUpdates();

    createGui(m_app->getForm());
}

ClusteredLightingScene::~ClusteredLightingScene()
{
    m_app->getForm()->remove("shadowMappingPanel");
}

void ClusteredLightingScene::resize(int width, int height)
{
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(width, height);
    m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 2);
}

void ClusteredLightingScene::update(float dt)
{
    m_cameraController->update(dt);
    const auto cameraParams = m_cameraController->getCameraParameters();

    m_transformBuffer->update(cameraParams.V, cameraParams.P);

    // TODO
    // m_lightSystem->update(m_cameraController->getCamera(), dt);

    m_resourceContext->getUniformBuffer("pbrUnifParams")->updateStagingBuffer(m_uniformMaterialParams);

    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(cameraParams);
}

void ClusteredLightingScene::render()
{
    m_renderGraph->clearCommandLists();
    m_renderGraph->buildCommandLists(m_renderNodes);
    m_renderGraph->executeCommandLists();
}

void ClusteredLightingScene::setRedAlbedo(double red)
{
    m_uniformMaterialParams.albedo.r = static_cast<float>(red);
}

void ClusteredLightingScene::setGreenAlbedo(double green)
{
    m_uniformMaterialParams.albedo.g = static_cast<float>(green);
}

void ClusteredLightingScene::setBlueAlbedo(double blue)
{
    m_uniformMaterialParams.albedo.b = static_cast<float>(blue);
}

void ClusteredLightingScene::setMetallic(double metallic)
{
    m_uniformMaterialParams.metallic = static_cast<float>(metallic);
}

void ClusteredLightingScene::setRoughness(double roughness)
{
    m_uniformMaterialParams.roughness = static_cast<float>(roughness);
}

RenderNode* ClusteredLightingScene::createRenderNode(std::string id, int transformIndex)
{
    TransformHandle handle{static_cast<uint16_t>(transformIndex), 0};
    auto node = std::make_unique<RenderNode>(*m_transformBuffer, handle);
    m_renderNodes.emplace(id, std::move(node));
    return m_renderNodes.at(id).get();
}

void ClusteredLightingScene::createCommonTextures()
{
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler(
        "nearestNeighbor",
        std::make_unique<VulkanSampler>(
            m_renderer->getDevice(), VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    imageCache.addSampler(
        "linearRepeat",
        std::make_unique<VulkanSampler>(
            m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f));
    imageCache.addSampler(
        "linearMipmap",
        std::make_unique<VulkanSampler>(
            m_renderer->getDevice(),
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            16.0f,
            5.0f));
    imageCache.addSampler(
        "linearClamp",
        std::make_unique<VulkanSampler>(
            m_renderer->getDevice(),
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            16.0f,
            11.0f));
    imageCache.addSampler(
        "linearMirrorRepeat",
        std::make_unique<VulkanSampler>(
            m_renderer->getDevice(),
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
            16.0f,
            11.0f));

    // Environment map
    LuaConfig config(m_renderer->getResourcesPath() / "Scripts/scene.lua");
    auto hdrName = config.get<std::string>("environmentMap").value_or("GreenwichPark") + ".hdr";
    auto envRefMap = createVulkanImage(
        *m_renderer,
        loadImage(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps" / hdrName, 4, FlipOnLoad::Y).unwrap(),
        VK_FORMAT_R32G32B32A32_SFLOAT);
    std::shared_ptr<VulkanImageView> envRefMapView = envRefMap->createView(VK_IMAGE_VIEW_TYPE_2D);

    auto [cubeMap, cubeMapView] = convertEquirectToCubeMap(m_renderer, envRefMapView, 1024);

    auto [diffEnv, diffEnvView] = setupDiffuseEnvMap(m_renderer, *cubeMapView, 64);
    imageCache.addImageWithView("envIrrMap", std::move(diffEnv), std::move(diffEnvView));
    auto [reflEnv, reflEnvView] = setupReflectEnvMap(m_renderer, *cubeMapView, 512);
    imageCache.addImageWithView("filteredMap", std::move(reflEnv), std::move(reflEnvView));

    imageCache.addImageWithView("cubeMap", std::move(cubeMap), std::move(cubeMapView));
    imageCache.addImageWithView("brdfLut", integrateBrdfLut(m_renderer));

    auto pbrPipeline = m_resourceContext->createPipeline(
        "pbrUnif", "PbrClusteredLights.lua", m_renderGraph->getRenderPass(MainPass), 0);
    auto pbrMaterial = m_resourceContext->createMaterial("pbrUnif", pbrPipeline);
    pbrMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    pbrMaterial->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));

    m_uniformMaterialParams.albedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    m_uniformMaterialParams.metallic = 0.0f;
    m_uniformMaterialParams.roughness = 0.1f;
    m_resourceContext->addUniformBuffer(
        "pbrUnifParams",
        std::make_unique<UniformBuffer>(m_renderer, m_uniformMaterialParams, BufferUpdatePolicy::PerFrame));
    pbrMaterial->writeDescriptor(0, 2, *m_resourceContext->getUniformBuffer("pbrUnifParams"));

    pbrMaterial->writeDescriptor(1, 0, *m_lightSystem->getPointLightBuffer());
    pbrMaterial->writeDescriptor(1, 1, *m_lightSystem->getLightIndexBuffer());

    pbrMaterial->writeDescriptor(2, 0, imageCache.getImageView("envIrrMap"), &imageCache.getSampler("linearClamp"));
    pbrMaterial->writeDescriptor(2, 1, imageCache.getImageView("filteredMap"), &imageCache.getSampler("linearMipmap"));
    pbrMaterial->writeDescriptor(2, 2, imageCache.getImageView("brdfLut"), &imageCache.getSampler("linearClamp"));

    pbrMaterial->writeDescriptor(3, 0, m_lightSystem->getTileGridViews(), nullptr, VK_IMAGE_LAYOUT_GENERAL);
}

void ClusteredLightingScene::createShaderball()
{
    // std::vector<VertexAttributeDescriptor> shadowVertexFormat = { VertexAttribute::Position };
    // std::vector<VertexAttributeDescriptor> pbrVertexFormat = { VertexAttribute::Position, VertexAttribute::Normal,
    // VertexAttribute::TexCoord, VertexAttribute::Tangent };

    // TriangleMesh mesh(m_renderer->getResourcesPath() / "Meshes/sponza_fixed.obj", pbrVertexFormat);
    // m_resourceContext->addGeometry("shaderBallPbr", std::make_unique<Geometry>(m_renderer, mesh, pbrVertexFormat));
    // m_resourceContext->addGeometry("shaderBallShadow", std::make_unique<Geometry>(m_renderer, mesh,
    // shadowVertexFormat));

    // auto pbrShaderBall = createRenderNode("pbrShaderBall", 3);
    // pbrShaderBall->transformPack->M = glm::scale(glm::vec3(5.f));
    // pbrShaderBall->geometry = m_resourceContext->getGeometry("shaderBallPbr");
    // pbrShaderBall->pass(MainPass).material = m_resourceContext->getMaterial("pbrUnif");

    ////for (uint32_t i = 0; i < CascadeCount; ++i)
    ////{
    ////    auto& subpass = pbrShaderBall->subpass(CsmPass, i);
    ////    subpass.geometry = m_resourceContext->getGeometry("shaderBallShadow");
    ////    subpass.material = m_resourceContext->getMaterial("cascadedShadowMap" + std::to_string(i));
    ////}

    // m_renderer->getDevice()->flushDescriptorUpdates();
}

void ClusteredLightingScene::createPlane()
{
    const VertexLayoutDescription pbrVertexFormat = {
        {VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent}
    };

    m_resourceContext->addGeometry(
        "floor",
        std::make_unique<Geometry>(*m_renderer, createPlaneMesh(flatten(pbrVertexFormat), 200.0f), pbrVertexFormat));

    auto floor = createRenderNode("floor", 0);
    floor->transformPack->M = glm::scale(glm::vec3(1.0, 1.0f, 1.0f));
    floor->geometry = m_resourceContext->getGeometry("floor");
    floor->pass(MainPass).material = m_resourceContext->getMaterial("pbrUnif");
    floor->pass(MainPass).setPushConstants(glm::vec2(100.0f));

    m_renderer->getDevice().flushDescriptorUpdates();
}

void ClusteredLightingScene::setupInput()
{
    m_app->getWindow().keyPressed += [this](Key key, int)
    {
        switch (key)
        {
        case Key::F5:
            m_resourceContext->recreatePipelines();
            break;
        }
    };
}

void ClusteredLightingScene::createGui(gui::Form* /*form*/) {}
} // namespace crisp
