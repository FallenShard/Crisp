#include <Crisp/Scenes/PbrScene.hpp>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/LuaConfig.hpp>

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Camera/TargetCameraController.hpp>

#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>
#include <Crisp/Vulkan/VulkanSampler.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>

#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/RenderPasses/DepthPass.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/Renderer/VulkanBufferUtils.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Models/Atmosphere.hpp>
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

#include <Crisp/GlmFormatters.hpp>
#include <Crisp/IO/GltfReader.hpp>
#include <Crisp/IO/MeshLoader.hpp>
#include <Crisp/IO/OpenEXRReader.hpp>
#include <Crisp/Math/Constants.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Profiler.hpp>

namespace crisp
{
namespace
{
const auto logger = createLoggerMt("PbrScene");

constexpr uint32_t ShadowMapSize = 1024;
constexpr uint32_t CascadeCount = 4;

constexpr const char* DepthPrePass = "DepthPrePass";
constexpr const char* MainPass = "mainPass";
constexpr const char* CsmPass = "csmPass";

constexpr const char* const PbrScenePanel = "pbrScenePanel";

float azimuth = 0.0f;
float altitude = 0.0f;

const std::vector<VertexAttributeDescriptor> PbrVertexFormat = {
    VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent};
const std::vector<VertexAttributeDescriptor> PosVertexFormat = {VertexAttribute::Position};

Material* createPbrUniformMaterial(
    ResourceContext& resourceContext,
    RenderGraph& renderGraph,
    const PbrParams& params,
    const LightSystem& lightSystem,
    const TransformBuffer& transformBuffer)
{
    auto& imageCache = resourceContext.imageCache;

    auto material = resourceContext.createMaterial("pbrUnif", "pbrUnif");

    // Configure global/pass/view descriptorSet.
    material->writeDescriptor(1, 0, *resourceContext.getUniformBuffer("camera"));
    material->writeDescriptor(1, 1, *lightSystem.getCascadedDirectionalLightBuffer());
    material->writeDescriptor(1, 2, renderGraph.getRenderPass(CsmPass), 0, &imageCache.getSampler("nearestNeighbor"));
    material->writeDescriptor(1, 3, imageCache.getImageView("envIrrMap"), imageCache.getSampler("linearClamp"));
    material->writeDescriptor(1, 4, imageCache.getImageView("filteredMap"), imageCache.getSampler("linearMipmap"));
    material->writeDescriptor(1, 5, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));

    // Configure material's parameters.
    resourceContext.createUniformBuffer("pbrUnifParams", params, BufferUpdatePolicy::PerFrame);
    material->writeDescriptor(2, 0, *resourceContext.getUniformBuffer("pbrUnifParams"));
    material->writeDescriptor(2, 1, imageCache.getImageView("sheenLut"), imageCache.getSampler("linearClamp"));

    // Configure draw item's parameters.
    material->writeDescriptor(0, 0, transformBuffer.getDescriptorInfo());
    return material;
}

Material* createPbrTexturedMaterial(
    const std::string& materialId,
    const std::string& materialName,
    Renderer& renderer,
    ResourceContext& resourceContext,
    const PbrParams& params,
    const LightSystem& lightSystem,
    const TransformBuffer& transformBuffer)
{
    auto& imageCache = resourceContext.imageCache;

    auto material = resourceContext.createMaterial("pbrTex" + materialId, "pbrTex");
    material->writeDescriptor(0, 0, transformBuffer.getDescriptorInfo());

    material->writeDescriptor(1, 0, *resourceContext.getUniformBuffer("camera"));
    material->writeDescriptor(1, 1, *lightSystem.getCascadedDirectionalLightBuffer());
    material->writeDescriptor(1, 2, 0, imageCache.getImageView("csmFrame0"), &imageCache.getSampler("nearestNeighbor"));
    material->writeDescriptor(1, 2, 1, imageCache.getImageView("csmFrame1"), &imageCache.getSampler("nearestNeighbor"));
    material->writeDescriptor(1, 3, imageCache.getImageView("envIrrMap"), imageCache.getSampler("linearClamp"));
    material->writeDescriptor(1, 4, imageCache.getImageView("filteredMap"), imageCache.getSampler("linearMipmap"));
    material->writeDescriptor(1, 5, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));

    const auto setMaterialTexture =
        [&material, &imageCache, &materialName, &renderer](const uint32_t index, const std::string_view texName)
    {
        const std::string key = fmt::format("{}-{}", materialName, texName);
        const std::string fallbackKey = fmt::format("default-{}", texName);
        material->writeDescriptor(
            2, index, imageCache.getImageView(key, fallbackKey), imageCache.getSampler("linearRepeat"));
    };

    setMaterialTexture(0, "diffuse");
    setMaterialTexture(1, "metallic");
    setMaterialTexture(2, "roughness");
    setMaterialTexture(3, "normal");
    setMaterialTexture(4, "ao");
    setMaterialTexture(5, "emissive");

    const std::string paramsBufferKey = {"pbrTex" + materialId + "Params"};
    resourceContext.createUniformBuffer(paramsBufferKey, params, BufferUpdatePolicy::PerFrame);
    material->writeDescriptor(2, 6, *resourceContext.getUniformBuffer(paramsBufferKey));

    return material;
}

} // namespace

PbrScene::PbrScene(Renderer* renderer, Application* app)
    : m_renderer(renderer)
    , m_app(app)
    , m_resourceContext(std::make_unique<ResourceContext>(renderer))
{
    setupInput();

    // Camera
    m_cameraController = std::make_unique<FreeCameraController>(app->getWindow());
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

    m_renderGraph = std::make_unique<RenderGraph>(m_renderer);

    m_renderGraph->addRenderPass(
        DepthPrePass, createDepthPass(m_renderer->getDevice(), m_renderer->getSwapChainExtent()));

    //// Main render pass
    m_renderGraph->addRenderPass(
        MainPass, createForwardLightingPass(m_renderer->getDevice(), renderer->getSwapChainExtent()));
    // m_renderGraph->addRenderTargetLayoutTransition(DepthPrePass, MainPass, 0);
    //// Shadow map pass
    m_renderGraph->addRenderPass(CsmPass, createShadowMapPass(m_renderer->getDevice(), ShadowMapSize, CascadeCount));
    m_renderGraph->addRenderTargetLayoutTransition(CsmPass, MainPass, 0, CascadeCount);

    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addImageView(
        "csmFrame0",
        m_renderGraph->getRenderPass(CsmPass).createRenderTargetView(m_renderer->getDevice(), 0, 0, CascadeCount));
    imageCache.addImageView(
        "csmFrame1",
        m_renderGraph->getRenderPass(CsmPass).createRenderTargetView(
            m_renderer->getDevice(), 0, CascadeCount, CascadeCount));

    // Wrap-up render graph definition
    m_renderGraph->addRenderTargetLayoutTransitionToScreen(MainPass, 0);

    m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)),
        ShadowMapSize,
        CascadeCount);

    // Object transforms
    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 100);

    createCommonTextures();

    for (uint32_t i = 0; i < CascadeCount; ++i)
    {
        std::string key = "cascadedShadowMap" + std::to_string(i);
        auto csmPipeline =
            m_resourceContext->createPipeline(key, "ShadowMap.lua", m_renderGraph->getRenderPass(CsmPass), i);
        auto csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
        csmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        csmMaterial->writeDescriptor(0, 1, *m_lightSystem->getCascadedDirectionalLightBuffer(i));
    }

    createPlane();
    createShaderballs();

    auto nodes = addAtmosphereRenderPasses(*m_renderGraph, *m_renderer, *m_resourceContext, MainPass);
    for (auto& [key, value] : nodes)
        m_renderNodes.emplace(std::move(key), std::move(value));
    m_renderer->setSceneImageView(m_renderGraph->getNode("RayMarchingPass").renderPass.get(), 0);

    m_renderGraph->sortRenderPasses().unwrap();
    // m_renderGraph->printExecutionOrder();

    m_skybox = std::make_unique<Skybox>(
        m_renderer,
        m_renderGraph->getRenderPass(MainPass),
        imageCache.getImageView("cubeMap"),
        imageCache.getSampler("linearClamp"));

    m_renderer->getDevice().flushDescriptorUpdates();

    // createGui(m_app->getForm());
}

PbrScene::~PbrScene()
{
    /*m_renderer->setSceneImageView(nullptr, 0);
    m_app->getForm()->remove(PbrScenePanel);*/
}

void PbrScene::resize(int width, int height)
{
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(width, height);
    m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);
}

void PbrScene::update(float dt)
{
    // Camera
    m_cameraController->update(dt);
    const auto camParams = m_cameraController->getCameraParameters();
    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(camParams);

    // Object transforms
    m_transformBuffer->update(camParams.V, camParams.P);
    m_lightSystem->update(m_cameraController->getCamera(), dt);
    m_skybox->updateTransforms(camParams.V, camParams.P);

    // m_resourceContext->getUniformBuffer("pbrUnifParams")->updateStagingBuffer(m_uniformMaterialParams);

    //// auto svp = m_lightSystem->getDirectionalLight().getProjectionMatrix() *
    //// m_lightSystem->getDirectionalLight().getViewMatrix();
    /////*atmosphere.gShadowmapViewProjMat = svp;
    //// atmosphere.gSkyViewProjMat = P * V;
    //// atmosphere.gSkyInvViewProjMat = glm::inverse(P * V);
    //// atmosphere.camera = m_cameraController->getCamera().getPosition();
    //// atmosphere.view_ray = m_cameraController->getCamera().getLookDirection();
    //// atmosphere.sun_direction = m_lightSystem->getDirectionalLight().getDirection();*/

    //

    //// atmosphere.gSkyViewProjMat = VP;

    //// atmosphere.gSkyViewProjMat = glm::scale(glm::vec3(1.0f, -1.0f, 1.0f)) * camParams.P * camParams.V;
    //// atmosphere.camera = m_cameraController->getCamera().getPosition();
    ///*atmosphere.view_ray = m_cameraController->getCamera().getLookDir();*/

    //// atmosphere.gSkyInvViewProjMat = glm::inverse(atmosphere.gSkyViewProjMat);
    //// m_resourceContext->getUniformBuffer("atmosphere")->updateStagingBuffer(atmosphere);

    //// commonConstantData.gSkyViewProjMat = atmosphere.gSkyViewProjMat;
    //// m_resourceContext->getUniformBuffer("atmosphereCommon")->updateStagingBuffer(commonConstantData);

    // atmoBuffer.cameraPosition = atmosphere.camera;
    AtmosphereParameters atmosphere{};
    const auto screenExtent = m_renderer->getSwapChainExtent();
    atmosphere.screenResolution = glm::vec2(screenExtent.width, screenExtent.height);
    m_resourceContext->getUniformBuffer("atmosphereBuffer")->updateStagingBuffer(atmosphere);
}

void PbrScene::render()
{
    m_renderGraph->clearCommandLists();
    m_renderGraph->buildCommandLists(m_renderNodes);
    m_renderGraph->addToCommandLists(m_skybox->getRenderNode());
    m_renderGraph->executeCommandLists();
}

void PbrScene::setRedAlbedo(double red)
{
    m_uniformMaterialParams.albedo.r = static_cast<float>(red);
}

void PbrScene::setGreenAlbedo(double green)
{
    m_uniformMaterialParams.albedo.g = static_cast<float>(green);
}

void PbrScene::setBlueAlbedo(double blue)
{
    m_uniformMaterialParams.albedo.b = static_cast<float>(blue);
}

void PbrScene::setMetallic(double metallic)
{
    m_uniformMaterialParams.metallic = static_cast<float>(metallic);
}

void PbrScene::setRoughness(double roughness)
{
    m_uniformMaterialParams.roughness = static_cast<float>(roughness);
}

void PbrScene::setUScale(double uScale)
{
    m_uniformMaterialParams.uvScale.s = static_cast<float>(uScale);
}

void PbrScene::setVScale(double vScale)
{
    m_uniformMaterialParams.uvScale.t = static_cast<float>(vScale);
}

void PbrScene::onMaterialSelected(const std::string& /*materialName*/)
{
    /*if (materialName == "Uniform")
    {
        m_renderNodes["pbrShaderBall"]->pass(MainPass).material = m_resourceContext->getMaterial("pbrUnif");
        return;
    }
    else
    {
        m_renderNodes["pbrShaderBall"]->pass(MainPass).material = m_resourceContext->getMaterial("pbrTexShaderBall");
    }

    auto material = m_resourceContext->getMaterial("pbrTexShaderBall");

    VulkanSampler* linearRepeatSampler = m_resourceContext->getSampler("linearRepeat");
    const std::vector<std::string> texNames = { "diffuse", "metallic", "roughness", "normal", "ao" };
    const std::vector<VkFormat> formats = { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM }; for (uint32_t i = 0; i < texNames.size(); ++i)
    {
        const std::string& filename = "PbrMaterials/" + materialName + "/" + texNames[i] + ".png";
        const auto& path = m_renderer->getResourcesPath() / "Textures" / filename;
        if (std::filesystem::exists(path))
        {
            std::string key = "shaderBall-" + texNames[i];
            m_resourceContext->addImageWithView(key, createTexture(m_renderer, filename, formats[i], true));
            material->writeDescriptor(1, 2 + i, *m_resourceContext->getImageView(key), linearRepeatSampler);
        }
        else
        {
            spdlog::warn("Texture type {} is using default values for '{}'", materialName, texNames[i]);
            std::string key = "default-" + texNames[i];
            material->writeDescriptor(1, 2 + i, *m_resourceContext->getImageView(key), linearRepeatSampler);
        }
    }
    m_renderer->getDevice()->flushDescriptorUpdates();*/
}

RenderNode* PbrScene::createRenderNode(std::string id, std::optional<int> transformIndex)
{
    if (!transformIndex)
    {
        return m_renderNodes.emplace(id, std::make_unique<RenderNode>()).first->second.get();
    }
    else
    {
        return m_renderNodes.emplace(id, std::make_unique<RenderNode>(*m_transformBuffer, transformIndex.value()))
            .first->second.get();
    }
}

void PbrScene::createCommonTextures()
{
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler("nearestNeighbor", createNearestClampSampler(m_renderer->getDevice()));
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

    // For textured pbr
    addDefaultPbrTexturesToImageCache(imageCache);

    m_resourceContext->createPipeline("pbrTex", "PbrTex.lua", m_renderGraph->getRenderPass(MainPass), 0);
    m_resourceContext->createPipeline("pbrUnif", "PbrUnif.lua", m_renderGraph->getRenderPass(MainPass), 0);

    // Environment map
    LuaConfig config(m_renderer->getResourcesPath() / "Scripts/scene.lua");
    auto hdrName = config.get<std::string>("environmentMap").value_or("GreenwichPark") + ".hdr";
    auto envRefMap = createEnvironmentMap(m_renderer, hdrName, VK_FORMAT_R32G32B32A32_SFLOAT, FlipOnLoad::Y);
    std::shared_ptr<VulkanImageView> envRefMapView = envRefMap->createView(VK_IMAGE_VIEW_TYPE_2D);

    auto [cubeMap, cubeMapView] = convertEquirectToCubeMap(m_renderer, envRefMapView, 1024);

    auto [diffEnv, diffEnvView] = setupDiffuseEnvMap(m_renderer, *cubeMapView, 64);
    imageCache.addImageWithView("envIrrMap", std::move(diffEnv), std::move(diffEnvView));
    auto [reflEnv, reflEnvView] = setupReflectEnvMap(m_renderer, *cubeMapView, 512);
    imageCache.addImageWithView("filteredMap", std::move(reflEnv), std::move(reflEnvView));

    imageCache.addImageWithView("cubeMap", std::move(cubeMap), std::move(cubeMapView));
    imageCache.addImageWithView("brdfLut", integrateBrdfLut(m_renderer));

    OpenEXRReader reader;
    std::vector<float> buffer;
    uint32_t w, h;
    reader.read(m_renderer->getResourcesPath() / "Textures/Sheen_E.exr", buffer, w, h);
    VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = VK_FORMAT_R32_SFLOAT;
    createInfo.extent = {w, h, 1u};
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto sheenLut =
        createTexture(m_renderer, buffer.size() * sizeof(float), buffer.data(), createInfo, VK_IMAGE_ASPECT_COLOR_BIT);
    imageCache.addImageWithView("sheenLut", std::move(sheenLut));
}

void PbrScene::createShaderballs()
{
    const std::vector<std::string> materials = {
        "Floor",
        "Grass",
        "Limestone",
        "Mahogany",
        "MetalGrid",
        "MixedMoss",
        "OrnateBrass",
        "PirateGold",
        "RedBricks",
        "RustedIron",
        "SteelPlate",
        "Snowdrift",
        "Hexstone",
    };

    auto [mesh, gltfTextures] =
        loadGltfModel(m_renderer->getResourcesPath() / "Meshes/DamagedHelmet/DamagedHelmet.gltf", PbrVertexFormat)
            .unwrap();
    PbrMaterial helmetMaterial{};
    helmetMaterial.key = "DamagedHelmet";
    helmetMaterial.textures = std::move(gltfTextures);
    addToImageCache(helmetMaterial.textures, helmetMaterial.key, m_resourceContext->imageCache);

    /*const TriangleMesh mesh(
        loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/ShaderBall_FWVN_PosX.obj", PbrVertexFormat));*/
    // spdlog::warn("{} {}", mesh.getBoundingBox().min, mesh.getBoundingBox().max);
    m_resourceContext->addGeometry("shaderBall", std::make_unique<Geometry>(m_renderer, mesh, PbrVertexFormat));
    m_resourceContext->addGeometry("shaderBallPosOnly", std::make_unique<Geometry>(m_renderer, mesh, PosVertexFormat));

    const int32_t columnCount = 1;
    const int32_t rowCount = 1;
    for (int32_t i = 0; i < rowCount; ++i)
    {
        for (int32_t j = 0; j < columnCount; ++j)
        {
            const int32_t linearIdx = i * columnCount + j;

            auto pbrShaderBall = createRenderNode("pbrShaderBall" + std::to_string(linearIdx), 3 + linearIdx);

            const glm::mat4 translation = glm::translate(glm::vec3(5.0f * j, -mesh.getBoundingBox().min.y, 5.0f * i));
            pbrShaderBall->transformPack->M = translation; // * glm::scale(glm::vec3(0.025f));
            pbrShaderBall->geometry = m_resourceContext->getGeometry("shaderBall");

            const std::string materialName = materials[linearIdx % materials.size()];
            const auto fullPath{m_renderer->getResourcesPath() / "Textures/PbrMaterials" / materialName};
            addToImageCache(loadPbrTextureGroup(fullPath), fullPath.stem().string(), m_resourceContext->imageCache);
            PbrParams params{};
            pbrShaderBall->pass(MainPass).material = createPbrTexturedMaterial(
                "ShaderBall" + std::to_string(linearIdx),
                helmetMaterial.key,
                *m_renderer,
                *m_resourceContext,
                params,
                *m_lightSystem,
                *m_transformBuffer);
            pbrShaderBall->pass(MainPass).setPushConstantView(m_uniformMaterialParams.uvScale);

            for (uint32_t c = 0; c < CascadeCount; ++c)
            {
                auto& subpass = pbrShaderBall->subpass(CsmPass, c);
                subpass.geometry = m_resourceContext->getGeometry("shaderBallPosOnly");
                subpass.material = m_resourceContext->getMaterial("cascadedShadowMap" + std::to_string(c));
            }
        }
    }

    m_renderer->getDevice().flushDescriptorUpdates();
}

void PbrScene::createPlane()
{
    m_resourceContext->addGeometry(
        "floor", std::make_unique<Geometry>(m_renderer, createPlaneMesh(PbrVertexFormat, 200.0f)));
    m_resourceContext->addGeometry(
        "floorPosOnly", std::make_unique<Geometry>(m_renderer, createPlaneMesh(PosVertexFormat, 200.0f)));

    auto floor = createRenderNode("floor", 0);
    floor->transformPack->M = glm::scale(glm::vec3(1.0, 1.0f, 1.0f));
    floor->geometry = m_resourceContext->getGeometry("floor");

    const auto fullPath{m_renderer->getResourcesPath() / "Textures/PbrMaterials" / "Hexstone"};
    addToImageCache(loadPbrTextureGroup(fullPath), fullPath.stem().string(), m_resourceContext->imageCache);
    PbrParams params{};
    params.uvScale = glm::vec2{100.0f, 100.0f};
    floor->pass(MainPass).material = createPbrTexturedMaterial(
        "floor", "Hexstone", *m_renderer, *m_resourceContext, params, *m_lightSystem, *m_transformBuffer);
    // floor->pass(MainPass).setPushConstants(glm::vec2(100.0f));

    /*floor->pass(DepthPrePass).pipeline = m_resourceContext->createPipeline("depthPrepass", "DepthPrepass.lua",
        m_renderGraph->getRenderPass(DepthPrePass), 0);
    floor->pass(DepthPrePass).material =
        m_resourceContext->createMaterial("depthPrepass", floor->pass(DepthPrePass).pipeline);
    floor->pass(DepthPrePass).material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    floor->pass(DepthPrePass).geometry = m_resourceContext->getGeometry("floorPosOnly");*/

    m_renderer->getDevice().flushDescriptorUpdates();
}

void PbrScene::setupInput()
{
    m_connectionHandlers.emplace_back(m_app->getWindow()->keyPressed.subscribe(
        [this](Key key, int)
        {
            switch (key)
            {
            case Key::F5:
                m_resourceContext->recreatePipelines();
                break;
            }
        }));
}

void PbrScene::createGui(gui::Form* form)
{
    using namespace gui;
    std::unique_ptr<Panel> panel = std::make_unique<Panel>(form);

    panel->setId(PbrScenePanel);
    panel->setPadding({20, 20});
    panel->setPosition({20, 40});
    panel->setVerticalSizingPolicy(SizingPolicy::WrapContent);
    panel->setHorizontalSizingPolicy(SizingPolicy::WrapContent);

    int y = 0;
    auto addLabeledSlider = [&](const std::string& labelText, double val, double minVal, double maxVal)
    {
        auto label = std::make_unique<Label>(form, labelText);
        label->setPosition({0, y});
        panel->addControl(std::move(label));
        y += 20;

        auto slider = std::make_unique<DoubleSlider>(form, minVal, maxVal);
        slider->setId(labelText + "Slider");
        slider->setAnchor(Anchor::TopCenter);
        slider->setOrigin(Origin::TopCenter);
        slider->setPosition({0, y});
        slider->setValue(val);
        slider->setIncrement(0.01f);

        DoubleSlider* sliderPtr = slider.get();
        panel->addControl(std::move(slider));
        y += 30;

        return sliderPtr;
    };

    addLabeledSlider("Azimuth", 0.0, 0.0, glm::pi<double>() * 2.0)->valueChanged += [](const double& v)
    {
        azimuth = static_cast<float>(v);
        /*atmosphere.sun_direction.x = std::cos(azimuth) * std::cos(altitude);
        atmosphere.sun_direction.y = std::sin(altitude);
        atmosphere.sun_direction.z = std::sin(azimuth) * std::cos(altitude);*/
    };
    addLabeledSlider("Altitude", 0.0, 0.0, glm::pi<double>() * 2.0)->valueChanged += [](const double& v)
    {
        altitude = static_cast<float>(v);
        /*atmosphere.sun_direction.x = std::cos(azimuth) * std::cos(altitude);
        atmosphere.sun_direction.y = std::sin(altitude);
        atmosphere.sun_direction.z = std::sin(azimuth) * std::cos(altitude);*/
    };

    addLabeledSlider("Roughness", m_uniformMaterialParams.roughness, 0.0, 1.0)
        ->valueChanged.subscribe<&PbrScene::setRoughness>(this);
    addLabeledSlider("Metallic", m_uniformMaterialParams.metallic, 0.0, 1.0)
        ->valueChanged.subscribe<&PbrScene::setMetallic>(this);
    addLabeledSlider("Red", m_uniformMaterialParams.albedo.r, 0.0, 1.0)
        ->valueChanged.subscribe<&PbrScene::setRedAlbedo>(this);
    addLabeledSlider("Green", m_uniformMaterialParams.albedo.g, 0.0, 1.0)
        ->valueChanged.subscribe<&PbrScene::setGreenAlbedo>(this);
    addLabeledSlider("Blue", m_uniformMaterialParams.albedo.b, 0.0, 1.0)
        ->valueChanged.subscribe<&PbrScene::setBlueAlbedo>(this);
    addLabeledSlider("U scale", m_uniformMaterialParams.uvScale.s, 1.0, 20.0)
        ->valueChanged.subscribe<&PbrScene::setUScale>(this);
    addLabeledSlider("V scale", m_uniformMaterialParams.uvScale.t, 1.0, 20.0)
        ->valueChanged.subscribe<&PbrScene::setVScale>(this);

    std::vector<std::string> materials;
    for (auto dir : std::filesystem::directory_iterator(m_renderer->getResourcesPath() / "Textures/PbrMaterials"))
        materials.push_back(dir.path().stem().string());
    materials.push_back("Uniform");

    auto comboBox = std::make_unique<gui::ComboBox>(form);
    comboBox->setId("materialComboBox");
    comboBox->setPosition({0, y});
    comboBox->setItems(materials);
    comboBox->itemSelected.subscribe<&PbrScene::onMaterialSelected>(this);
    panel->addControl(std::move(comboBox));
    y += 40;

    auto floorCheckBox = std::make_unique<gui::CheckBox>(form);
    floorCheckBox->setChecked(true);
    floorCheckBox->setText("Show Floor");
    floorCheckBox->setPosition({0, y});
    panel->addControl(std::move(floorCheckBox));

    form->add(std::move(panel));

    /*auto memoryUsageBg = std::make_unique<Panel>(form);
    memoryUsageBg->setSizingPolicy(SizingPolicy::FillParent, SizingPolicy::Fixed);
    memoryUsageBg->setSizeHint({ 0.0f, 20.0f });
    memoryUsageBg->setPosition({ 0, 20 });
    memoryUsageBg->setColor(glm::vec3(0.2f, 0.4f, 0.2f));

    auto device = m_renderer->getDevice();
    auto memAlloc = device->getMemoryAllocator();
    auto heap = memAlloc->getDeviceImageHeap();

    int i = 0;
    for (const auto& block : heap->memoryBlocks)
    {
        for (const auto [off, size] : block.usedChunks)
        {
            auto alloc = std::make_unique<Panel>(form);
            alloc->setAnchor(Anchor::CenterLeft);
            alloc->setOrigin(Origin::CenterLeft);
            alloc->setSizingPolicy(SizingPolicy::Fixed, SizingPolicy::Fixed);
            float s = static_cast<double>(size) / (heap->memoryBlocks.size() * heap->blockSize) * 1920;
            float x = static_cast<double>(off + i * heap->blockSize) / (heap->memoryBlocks.size() * heap->blockSize) *
    1920; alloc->setSizeHint({ s, 18.0f }); alloc->setPosition({ x, 0 }); alloc->setColor(glm::vec3(0.2f, 1.0f, 0.2f));
            memoryUsageBg->addControl(std::move(alloc));

            spdlog::info("Block {}: [{}, {}]", i, off, size);
        }

        ++i;
    }

    form->add(std::move(memoryUsageBg));*/
}
} // namespace crisp