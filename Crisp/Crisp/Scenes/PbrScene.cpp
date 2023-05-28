#include <Crisp/Scenes/PbrScene.hpp>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Checks.hpp>
#include <Crisp/IO/FileUtils.hpp>
#include <Crisp/IO/JsonUtils.hpp>
#include <Crisp/Image/Io/Utils.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>
#include <Crisp/Utils/LuaConfig.hpp>

#include <imgui.h>

namespace crisp
{
namespace
{
const auto logger = createLoggerMt("PbrScene");

constexpr const char* DepthPrePass = "depthPrePass";
constexpr const char* ForwardLightingPass = "forwardPass";
constexpr const char* OutputPass = ForwardLightingPass;

constexpr uint32_t ShadowMapSize = 1024;
constexpr uint32_t CascadeCount = 4;
constexpr std::array<const char*, CascadeCount> CsmPasses = {
    "csmPass0",
    "csmPass1",
    "csmPass2",
    "csmPass3",
};

constexpr const char* const PbrScenePanel = "pbrScenePanel";

float azimuth = 0.0f;
float altitude = 0.0f;

const VertexLayoutDescription PbrVertexFormat = {
    {VertexAttribute::Position},
    {VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent}
};

void setPbrMaterialSceneParams(
    Material& material, const ResourceContext& resourceContext, const LightSystem& lightSystem)
{
    const auto& imageCache = resourceContext.imageCache;
    const auto& envLight = *lightSystem.getEnvironmentLight();
    material.writeDescriptor(1, 0, *resourceContext.getUniformBuffer("camera"));
    material.writeDescriptor(1, 1, *lightSystem.getCascadedDirectionalLightBuffer());
    material.writeDescriptor(1, 2, envLight.getDiffuseMapView(), imageCache.getSampler("linearClamp"));
    material.writeDescriptor(1, 3, envLight.getSpecularMapView(), imageCache.getSampler("linearMipmap"));
    material.writeDescriptor(1, 4, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));
    material.writeDescriptor(1, 5, 0, imageCache.getImageView("csmFrame0"), &imageCache.getSampler("nearestNeighbor"));
    material.writeDescriptor(1, 5, 1, imageCache.getImageView("csmFrame1"), &imageCache.getSampler("nearestNeighbor"));
    material.writeDescriptor(1, 6, imageCache.getImageView("sheenLut"), imageCache.getSampler("linearClamp"));
}

Material* createPbrMaterial(
    const std::string& materialId,
    const std::string& materialKey,
    ResourceContext& resourceContext,
    const PbrParams& params,
    const TransformBuffer& transformBuffer)
{
    auto& imageCache = resourceContext.imageCache;

    auto material = resourceContext.createMaterial("pbrTex" + materialId, "pbrTex");
    material->writeDescriptor(0, 0, transformBuffer.getDescriptorInfo());

    const auto setMaterialTexture =
        [&material, &imageCache, &materialKey](const uint32_t index, const std::string_view texName)
    {
        const std::string key = fmt::format("{}-{}", materialKey, texName);
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

    const std::string paramsBufferKey{fmt::format("{}-params", materialId)};
    material->writeDescriptor(
        2, 6, *resourceContext.createUniformBuffer(paramsBufferKey, params, BufferUpdatePolicy::PerFrame));

    return material;
}

std::unique_ptr<VulkanRenderPass> createRedChannelPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, const VkExtent2D renderArea)
{
    std::vector<RenderTarget*> renderTargets(1);
    renderTargets[0] = renderTargetCache.addRenderTarget(
        "RedChannelColor",
        RenderTargetBuilder()
            .setFormat(VK_FORMAT_R16_SFLOAT)
            .setBuffered(true)
            .configureColorRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT, {1.0f, 0.5f, 0.0f, 1.0f})
            .setSize(renderArea, true)
            .create(device));

    return RenderPassBuilder()
        .setAttachmentCount(1)
        .setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
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
        .create(device, renderArea, renderTargets);
}

} // namespace

PbrScene::PbrScene(Renderer* renderer, Application* app)
    : AbstractScene(app, renderer)
{
    setupInput();

    m_cameraController = std::make_unique<FreeCameraController>(app->getWindow());
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);
    m_renderer->getDebugMarker().setObjectName(m_resourceContext->getUniformBuffer("camera")->get(), "cameraBuffer");

    // m_renderGraph->addRenderPass(
    //     DepthPrePass,
    //     createDepthPass(
    //         m_renderer->getDevice(), m_resourceContext->renderTargetCache, m_renderer->getSwapChainExtent()));
    m_renderGraph->addRenderPass(
        ForwardLightingPass,
        createForwardLightingPass(
            m_renderer->getDevice(), m_resourceContext->renderTargetCache, m_renderer->getSwapChainExtent()));
    // m_renderGraph->addDependency(DepthPrePass, ForwardLightingPass, 0);
    // Cascades for the shadow map pass
    for (uint32_t i = 0; i < CsmPasses.size(); ++i)
    {
        m_renderGraph->addRenderPass(
            CsmPasses[i],
            createShadowMapPass(m_renderer->getDevice(), m_resourceContext->renderTargetCache, ShadowMapSize, i));
        m_renderGraph->addDependency(CsmPasses[i], ForwardLightingPass, 0);
    }

    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addImageView(
        "csmFrame0",
        createView(
            *m_resourceContext->renderTargetCache.get("ShadowMap")->image,
            VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            0,
            CascadeCount));
    imageCache.addImageView(
        "csmFrame1",
        createView(
            *m_resourceContext->renderTargetCache.get("ShadowMap")->image,
            VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            CascadeCount,
            CascadeCount));

    // 1. Define the render pass for execution of the post processing effect.
    const std::string myPostProcessName{"redChannel"};
    m_renderGraph->addRenderPass(
        myPostProcessName,
        createRedChannelPass(
            m_renderer->getDevice(), m_resourceContext->renderTargetCache, m_renderer->getSwapChainExtent()));
    // 2. Add the dependency for the input.
    m_renderGraph->addDependency(ForwardLightingPass, myPostProcessName, 0);

    // 3. Create the pipeline and the material and assign the full screen geometry for rasterization passes.
    auto rcp = m_resourceContext->createPipeline(
        myPostProcessName, "RedChannel.json", m_renderGraph->getRenderPass(myPostProcessName), 0);
    auto redChannelNode = createRenderNode(myPostProcessName, false);
    redChannelNode->geometry = m_renderer->getFullScreenGeometry();
    redChannelNode->pass(myPostProcessName).pipeline = rcp;
    redChannelNode->pass(myPostProcessName).material = m_resourceContext->createMaterial(myPostProcessName, rcp);
    // 4. (Optionally) Set the output to the screen to be this post-processing pass.

    // Wrap-up render graph definition
    m_renderGraph->addDependencyToPresentation(myPostProcessName, 0);

    m_renderer->setSceneImageView(m_renderGraph->getNode(ForwardLightingPass).renderPass.get(), 0);

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)),
        ShadowMapSize,
        CascadeCount);

    // Object transforms
    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 100);
    m_renderer->getDebugMarker().setObjectName(m_transformBuffer->getUniformBuffer()->get(), "transformBuffer");

    createCommonTextures();

    // 5. Assign the post-processing effect's material parameters.
    redChannelNode->pass(myPostProcessName)
        .material->writeDescriptor(
            0,
            0,
            m_renderGraph->getRenderPass(ForwardLightingPass),
            0,
            &m_resourceContext->imageCache.getSampler("linearClamp"));

    for (uint32_t i = 0; i < CascadeCount; ++i)
    {
        std::string key = "cascadedShadowMap" + std::to_string(i);
        auto csmPipeline =
            m_resourceContext->createPipeline(key, "ShadowMap.json", m_renderGraph->getRenderPass(CsmPasses[i]), 0);
        auto csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
        csmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        csmMaterial->writeDescriptor(0, 1, *m_lightSystem->getCascadedDirectionalLightBuffer(i));
    }

    createPlane();
    createSceneObject();

    /*auto nodes = addAtmosphereRenderPasses(
        *m_renderGraph, *m_renderer, *m_resourceContext, m_resourceContext->renderTargetCache, ForwardLightingPass);
    for (auto& [key, value] : nodes)
        m_renderNodes.emplace(std::move(key), std::move(value));*/
    // m_renderer->setSceneImageView(m_renderGraph->getNode("RayMarchingPass").renderPass.get(), 0);

    m_renderGraph->sortRenderPasses().unwrap();
    m_renderGraph->printExecutionOrder();

    m_skybox = m_lightSystem->getEnvironmentLight()->createSkybox(
        *m_renderer, m_renderGraph->getRenderPass(ForwardLightingPass), imageCache.getSampler("linearClamp"));

    m_renderer->getDevice().flushDescriptorUpdates();
}

void PbrScene::resize(int width, int height)
{
    m_cameraController->onViewportResized(width, height);

    m_resourceContext->renderTargetCache.resizeRenderTargets(m_renderer->getDevice(), m_renderer->getSwapChainExtent());

    m_renderGraph->resize(width, height);
    m_renderer->setSceneImageView(m_renderGraph->getNode(ForwardLightingPass).renderPass.get(), 0);
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

    m_resourceContext->getUniformBuffer("sceneObject-params")->updateStagingBuffer(m_uniformMaterialParams);

    ////// auto svp = m_lightSystem->getDirectionalLight().getProjectionMatrix() *
    ////// m_lightSystem->getDirectionalLight().getViewMatrix();
    ///////*atmosphere.gShadowmapViewProjMat = svp;
    ////// atmosphere.gSkyViewProjMat = P * V;
    ////// atmosphere.gSkyInvViewProjMat = glm::inverse(P * V);
    ////// atmosphere.camera = m_cameraController->getCamera().getPosition();
    ////// atmosphere.view_ray = m_cameraController->getCamera().getLookDirection();
    ////// atmosphere.sun_direction = m_lightSystem->getDirectionalLight().getDirection();*/

    ////

    ////// atmosphere.gSkyViewProjMat = VP;

    ////// atmosphere.gSkyViewProjMat = glm::scale(glm::vec3(1.0f, -1.0f, 1.0f)) * camParams.P * camParams.V;
    ////// atmosphere.camera = m_cameraController->getCamera().getPosition();
    /////*atmosphere.view_ray = m_cameraController->getCamera().getLookDir();*/

    ////// atmosphere.gSkyInvViewProjMat = glm::inverse(atmosphere.gSkyViewProjMat);
    ////// m_resourceContext->getUniformBuffer("atmosphere")->updateStagingBuffer(atmosphere);

    ////// commonConstantData.gSkyViewProjMat = atmosphere.gSkyViewProjMat;
    ////// m_resourceContext->getUniformBuffer("atmosphereCommon")->updateStagingBuffer(commonConstantData);

    //// atmoBuffer.cameraPosition = atmosphere.camera;
    /*AtmosphereParameters atmosphere{};
    const auto screenExtent = m_renderer->getSwapChainExtent();
    atmosphere.screenResolution = glm::vec2(screenExtent.width, screenExtent.height);
    m_resourceContext->getUniformBuffer("atmosphereBuffer")->updateStagingBuffer(atmosphere);*/
}

void PbrScene::render()
{
    m_renderGraph->clearCommandLists();
    m_renderGraph->buildCommandLists(m_renderNodes);
    m_renderGraph->addToCommandLists(m_skybox->getRenderNode());
    m_renderGraph->executeCommandLists();
}

void PbrScene::renderGui()
{
    ImGui::Begin("Settings");
    ImGui::SliderFloat("Roughness", &m_uniformMaterialParams.roughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Metallic", &m_uniformMaterialParams.metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Red", &m_uniformMaterialParams.albedo.r, 0.0f, 1.0f);
    ImGui::SliderFloat("Green", &m_uniformMaterialParams.albedo.g, 0.0f, 1.0f);
    ImGui::SliderFloat("Blue", &m_uniformMaterialParams.albedo.b, 0.0f, 1.0f);
    ImGui::SliderFloat("U Scale", &m_uniformMaterialParams.uvScale.s, 1.0f, 20.0f);
    ImGui::SliderFloat("V Scale", &m_uniformMaterialParams.uvScale.t, 1.0f, 20.0f);

    std::vector<std::string> envMapNames;
    for (const auto& dir :
         std::filesystem::directory_iterator(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps"))
        envMapNames.push_back(dir.path().stem().string());

    if (ImGui::BeginCombo("Environment Light", m_lightSystem->getEnvironmentLight()->getName().c_str()))
    {
        for (const auto& envMap : envMapNames)
        {
            const bool isSelected{envMap == m_lightSystem->getEnvironmentLight()->getName()};
            if (ImGui::Selectable(envMap.c_str(), isSelected))
            {
                setEnvironmentMap(envMap);
            }
            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Checkbox("Show Floor", &m_showFloor))
    {
        m_renderNodes["floor"]->isVisible = m_showFloor;
    }

    ImGui::End();

    // std::vector<std::string> materials;
    // for (const auto& dir :
    //      std::filesystem::directory_iterator(m_renderer->getResourcesPath() / "Textures/PbrMaterials"))
    //     materials.push_back(dir.path().stem().string());
    // materials.push_back("Uniform");

    // auto comboBox = std::make_unique<gui::ComboBox>(form);
    // comboBox->setId("materialComboBox");
    // comboBox->setItems(materials);
    // comboBox->itemSelected.subscribe<&PbrScene::onMaterialSelected>(this);
    // panel->addControl(std::move(comboBox));

    // form->add(std::move(panel));
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

void PbrScene::onMaterialSelected(const std::string& materialName)
{
    m_renderer->finish();

    const auto materialPath{m_renderer->getResourcesPath() / "Textures/PbrMaterials" / materialName};
    PbrMaterial pbrMaterial{};
    pbrMaterial.name = materialPath.stem().string();

    auto shaderBallMaterial = m_resourceContext->getMaterial("pbrTexshaderBall");

    auto& imageCache{m_resourceContext->imageCache};
    const auto setMaterialTexture =
        [&shaderBallMaterial, &imageCache, &pbrMaterial, this](const uint32_t index, const std::string_view texName)
    {
        const std::string key = fmt::format("{}-{}", pbrMaterial.name, texName);
        const std::string fallbackKey = fmt::format("default-{}", texName);
        shaderBallMaterial->writeDescriptor(
            2, index, imageCache.getImageView(key, fallbackKey), imageCache.getSampler("linearRepeat"));
    };

    if (pbrMaterial.name != "Grass")
    {
        addPbrTexturesToImageCache(loadPbrTextureGroup(materialPath), pbrMaterial.name, imageCache);
    }
    if (m_shaderBallPbrMaterialKey != "Grass")
    {
        removePbrTexturesFromImageCache(m_shaderBallPbrMaterialKey, imageCache);
    }
    setMaterialTexture(0, "diffuse");
    setMaterialTexture(1, "metallic");
    setMaterialTexture(2, "roughness");
    setMaterialTexture(3, "normal");
    setMaterialTexture(4, "ao");
    setMaterialTexture(5, "emissive");
    m_renderer->getDevice().flushDescriptorUpdates();

    m_shaderBallPbrMaterialKey = pbrMaterial.name;
}

RenderNode* PbrScene::createRenderNode(std::string id, bool hasTransform)
{
    if (!hasTransform)
    {
        return m_renderNodes.emplace(id, std::make_unique<RenderNode>()).first->second.get();
    }
    else
    {
        const auto transformHandle{m_transformBuffer->getNextIndex()};
        return m_renderNodes.emplace(id, std::make_unique<RenderNode>(*m_transformBuffer, transformHandle))
            .first->second.get();
    }
}

void PbrScene::createCommonTextures()
{
    constexpr float kAnisotropy{16.0f};
    constexpr float kMaxLod{9.0f};
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler("nearestNeighbor", createNearestClampSampler(m_renderer->getDevice()));
    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), kAnisotropy));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy, kMaxLod));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy));
    addPbrTexturesToImageCache(createDefaultPbrTextureGroup(), "default", imageCache);

    m_resourceContext->createPipeline("pbrTex", "PbrTex.json", m_renderGraph->getRenderPass(ForwardLightingPass), 0);

    LuaConfig config(m_renderer->getResourcesPath() / "Scripts/scene.lua");
    setEnvironmentMap(config.get<std::string>("environmentMap").value_or("GreenwichPark"));
    imageCache.addImageWithView("brdfLut", integrateBrdfLut(m_renderer));
    imageCache.addImageWithView("sheenLut", createSheenLookup(*m_renderer, m_renderer->getResourcesPath()));
}

void PbrScene::setEnvironmentMap(const std::string& envMapName)
{
    m_lightSystem->setEnvironmentMap(
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps" / envMapName).unwrap(),
        envMapName);
}

void PbrScene::createSceneObject()
{
    // m_cameraController->setTarget(glm::vec3(0.0f, 1.0f, 0.0f));

    const std::string entityName{"sceneObject"};
    auto sceneObject = createRenderNode(entityName, true);
    const bool loadHelmet = true;
    TriangleMesh mesh{};
    PbrMaterial material{};
    if (loadHelmet)
    {
        auto renderObjects =
            loadGltfModel(
                m_renderer->getResourcesPath() / "Meshes/DamagedHelmet/DamagedHelmet.gltf", flatten(PbrVertexFormat))
                .unwrap();

        mesh = std::move(renderObjects.at(0).mesh);

        material = std::move(renderObjects.at(0).material);
        material.name = "DamagedHelmet";

        const glm::mat4 translation = glm::translate(glm::vec3(0.0f, -mesh.getBoundingBox().min.y, 0.0f));
        sceneObject->transformPack->M =
            translation * glm::rotate(glm::pi<float>() * 0.5f, glm ::vec3(1.0f, 0.0f, 0.0f));
    }
    else
    {
        auto [triMesh, materials] =
            loadTriangleMeshAndMaterial(
                m_renderer->getResourcesPath() / "Meshes/vokselia_spawn.obj", flatten(PbrVertexFormat))
                .unwrap();
        mesh = std::move(triMesh);

        material.name = "vokselia";
        material.textures = createDefaultPbrTextureGroup();
        material.textures.albedo =
            loadImage(m_renderer->getResourcesPath() / "Meshes/vokselia_spawn.png", 4, FlipOnLoad::Y).unwrap();

        const glm::mat4 translation = glm::translate(glm::vec3(0.0f, -mesh.getBoundingBox().min.y, 0.0f));
        const float maxDimLength = mesh.getBoundingBox().getMaximumExtent();
        sceneObject->transformPack->M = translation * glm::scale(glm::vec3(10.0f / maxDimLength));
    }

    m_shaderBallPbrMaterialKey = material.name;
    addPbrTexturesToImageCache(material.textures, material.name, m_resourceContext->imageCache);

    m_resourceContext->addGeometry(entityName, std::make_unique<Geometry>(*m_renderer, mesh, PbrVertexFormat));
    sceneObject->geometry = m_resourceContext->getGeometry(entityName);
    sceneObject->pass(ForwardLightingPass).material =
        createPbrMaterial(entityName, material.name, *m_resourceContext, material.params, *m_transformBuffer);
    setPbrMaterialSceneParams(*sceneObject->pass(ForwardLightingPass).material, *m_resourceContext, *m_lightSystem);

    for (uint32_t c = 0; c < CascadeCount; ++c)
    {
        auto& subpass = sceneObject->pass(CsmPasses[c]);
        subpass.setGeometry(m_resourceContext->getGeometry(entityName), 0, 1);
        subpass.material = m_resourceContext->getMaterial("cascadedShadowMap" + std::to_string(c));
        CRISP_CHECK(subpass.material->getPipeline()->getVertexLayout().isSubsetOf(subpass.geometry->getVertexLayout()));
    }

    m_renderer->getDevice().flushDescriptorUpdates();
}

void PbrScene::createPlane()
{
    const std::string entityName{"floor"};
    const TriangleMesh planeMesh{createPlaneMesh(flatten(PbrVertexFormat), 200.0f)};
    m_resourceContext->addGeometry(entityName, std::make_unique<Geometry>(*m_renderer, planeMesh, PbrVertexFormat));

    const auto materialPath{m_renderer->getResourcesPath() / "Textures/PbrMaterials" / "Grass"};
    PbrMaterial material{};
    material.name = materialPath.stem().string();
    material.params.uvScale = glm::vec2(100.0f, 100.0f);
    material.textures = loadPbrTextureGroup(materialPath);
    addPbrTexturesToImageCache(material.textures, material.name, m_resourceContext->imageCache);

    auto floor = createRenderNode(entityName, true);
    floor->transformPack->M = glm::scale(glm::vec3(1.0, 1.0f, 1.0f));
    floor->geometry = m_resourceContext->getGeometry(entityName);
    floor->pass(ForwardLightingPass).material =
        createPbrMaterial(entityName, material.name, *m_resourceContext, material.params, *m_transformBuffer);
    setPbrMaterialSceneParams(*floor->pass(ForwardLightingPass).material, *m_resourceContext, *m_lightSystem);

    CRISP_CHECK(floor->pass(ForwardLightingPass)
                    .material->getPipeline()
                    ->getVertexLayout()
                    .isSubsetOf(floor->geometry->getVertexLayout()));
}

void PbrScene::setupInput()
{
    m_connectionHandlers.emplace_back(m_app->getWindow().keyPressed.subscribe(
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
} // namespace crisp