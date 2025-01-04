#include <Crisp/Scenes/NormalMappingScene.hpp>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>

#include <Crisp/Camera/FreeCameraController.hpp>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

#include <Crisp/Image/Io/Utils.hpp>

#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Renderer/VulkanBufferUtils.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Models/Skybox.hpp>

#include <Crisp/Lights/DirectionalLight.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Lights/LightSystem.hpp>

#include <Crisp/Gui/Button.hpp>
#include <Crisp/Gui/CheckBox.hpp>
#include <Crisp/Gui/ComboBox.hpp>
#include <Crisp/Gui/Form.hpp>
#include <Crisp/Gui/Label.hpp>
#include <Crisp/Gui/Panel.hpp>
#include <Crisp/Gui/Slider.hpp>

#include <Crisp/Math/Constants.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>

namespace crisp {
namespace {
static constexpr uint32_t ShadowMapSize = 2048;
static constexpr uint32_t CascadeCount = 4;

static constexpr const char* MainPass = "mainPass";
static constexpr const char* CsmPass = "csmPass";

bool animationFrozen = true;
} // namespace

NormalMappingScene::NormalMappingScene(Renderer* renderer, Window* window)
    : Scene(renderer, window) {
    setupInput();

    // Camera
    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);
    // m_cameraController->getCamera().setPosition(glm::vec3(5.0f,
    // 5.0f, 5.0f));
    // m_cameraController->getCamera().setTarget(glm::vec3(0.0f));

    m_renderGraph = std::make_unique<RenderGraph>(m_renderer);

    // Main render pass
    m_renderGraph->addRenderPass(
        MainPass,
        createForwardLightingPass(
            m_renderer->getDevice(), m_resourceContext->renderTargetCache, renderer->getSwapChainExtent()));

    // Wrap-up render graph definition
    m_renderGraph->addDependency(MainPass, "SCREEN", 0);
    m_renderGraph->sortRenderPasses().unwrap();
    m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)),
        ShadowMapSize,
        0);

    // Object transforms
    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 10002);

    createPlane();

    m_renderer->getDevice().flushDescriptorUpdates();

    createGui();
}

NormalMappingScene::~NormalMappingScene() {}

void NormalMappingScene::resize(int width, int height) {
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(width, height);
    m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);
}

void NormalMappingScene::update(float dt) {
    m_cameraController->update(dt);
    const CameraParameters cameraParams = m_cameraController->getCameraParameters();

    static float angle = 0.0f;
    if (!animationFrozen) {
        angle += dt;
    }

    m_renderNodeList[m_renderNodeMap["nanosuit"]]->transformPack->M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f));

    m_transformBuffer->update(cameraParams.V, cameraParams.P);

    // TODO
    // m_lightSystem->update(m_cameraController->getCamera(),
    // dt);

    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer2(cameraParams);
}

void NormalMappingScene::render() {
    m_renderGraph->clearCommandLists();
    m_renderGraph->buildCommandLists(m_renderNodeList);
    m_renderGraph->executeCommandLists();
}

RenderNode* NormalMappingScene::createRenderNode(std::string id, int transformIndex) {
    // auto node =
    // std::make_unique<RenderNode>(*m_transformBuffer,
    // transformIndex);
    // m_renderNodes.emplace(id,
    // std::move(node)); return
    // m_renderNodes.at(id).get();
    const TransformHandle handle{static_cast<uint16_t>(transformIndex), 0};
    m_renderNodeList.push_back(std::make_unique<RenderNode>(*m_transformBuffer, handle));
    m_renderNodeMap[id] = m_renderNodeList.size() - 1;
    return m_renderNodeList.back().get();
}

void NormalMappingScene::createPlane() {
    auto& imageCache = m_resourceContext->imageCache;
    m_resourceContext->addGeometry("floor", createFromMesh(*m_renderer, createPlaneMesh(10.0f, 1.0f), kPbrVertexFormat));

    imageCache.addSampler(
        "linearRepeat",
        std::make_unique<VulkanSampler>(
            m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f));
    imageCache.addImageWithView(
        "normalMap",
        createVulkanImage(
            *m_renderer,
            loadImage(m_renderer->getResourcesPath() / "Textures/PbrMaterials/Hexstone/normal.png", 4).unwrap(),
            VK_FORMAT_R8G8B8A8_UNORM));
    imageCache.addImageWithView(
        "diffuseMap",
        createVulkanImage(
            *m_renderer,
            loadImage(m_renderer->getResourcesPath() / "Textures/PbrMaterials/Hexstone/diffuse.png", 4).unwrap(),
            VK_FORMAT_R8G8B8A8_SRGB));
    imageCache.addImageWithView(
        "specularMap",
        createVulkanImage(
            *m_renderer,
            loadImage(m_renderer->getResourcesPath() / "Textures/PbrMaterials/Hexstone/metallic.png", 4).unwrap(),
            VK_FORMAT_R8G8B8A8_UNORM));

    VulkanPipeline* pipeline =
        m_resourceContext->createPipeline("normalMap", "NormalMap.lua", m_renderGraph->getRenderPass(MainPass), 0);
    Material* material = m_resourceContext->createMaterial("normalMap", pipeline);
    material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    material->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));
    material->writeDescriptor(
        1, 0, imageCache.getImageView("normalMap").getDescriptorInfo(&imageCache.getSampler("linearRepeat")));
    material->writeDescriptor(
        1, 1, imageCache.getImageView("diffuseMap").getDescriptorInfo(&imageCache.getSampler("linearRepeat")));
    material->writeDescriptor(
        1, 2, imageCache.getImageView("specularMap").getDescriptorInfo(&imageCache.getSampler("linearRepeat")));

    auto floor = createRenderNode("floor", 0);
    floor->transformPack->M =
        glm::rotate(glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(1.0, 1.0f, 1.0f));
    floor->geometry = m_resourceContext->getGeometry("floor");
    floor->pass(MainPass).material = material;

    // m_resourceContext->addGeometry("sphere",
    // std::make_unique<Geometry>(m_renderer,
    // createPlaneMesh(pbrVertexFormat, 10.0f)));
    //
    // auto sphere =
    // createRenderNode("sphere", 1);
    // sphere->transformPack->M =
    // glm::translate(glm::vec3(0.0f, 1.0f,
    // 0.0f)); sphere->geometry =
    // m_resourceContext->getGeometry("sphere");

    // NANOSUIT
    TriangleMesh mesh(
        loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/nanosuit/nanosuit.obj ", flatten(kPbrVertexFormat))
            .unwrap());
    mesh.normalizeToUnitBox();
    mesh.transform(glm::translate(glm::vec3(0.0f, 0.5f, 0.0f)));
    m_resourceContext->addGeometry("nanosuit", createFromMesh(*m_renderer, mesh, kPbrVertexFormat));

    // Leg, Hand, Glass, Arm, Helmet,
    // Body
    auto nanosuit = createRenderNode("nanosuit", 1);
    // nanosuit->transformPack->M =
    // glm::rotate(glm::radians(0.0f),
    // glm::vec3(1.0f, 0.0f, 0.0f)) *
    // glm::scale(glm::vec3(5.0f));
    nanosuit->geometry = m_resourceContext->getGeometry("nanosuit");
    // nanosuit->pass(MainPass).material
    // = material;

    std::vector<std::string> parts = {"leg", "hand", "glass", "arm", "helmet", "body"};
    for (int i = 1; i < mesh.getViews().size(); ++i) {
        std::string normalMapKey = "partNormalMap" + std::to_string(i);
        std::string diffuseMapKey = "partDiffuseMap" + std::to_string(i);
        std::string specularMapKey = "partSpecularMap" + std::to_string(i);

        std::string normalMapFilename = parts[i - 1] + "_showroom_ddn.png";
        std::string diffuseMapFilename = parts[i - 1] + "_dif.png";
        std::string specularMapFilename = parts[i - 1] + "_showroom_spec.png";

        m_resourceContext->imageCache.addImageWithView(
            normalMapKey,
            createVulkanImage(
                *m_renderer,
                loadImage(m_renderer->getResourcesPath() / "Textures/nanosuit" / normalMapFilename, 4, FlipAxis::Y)
                    .unwrap(),
                VK_FORMAT_R8G8B8A8_UNORM));
        m_resourceContext->imageCache.addImageWithView(
            diffuseMapKey,
            createVulkanImage(
                *m_renderer,
                loadImage(m_renderer->getResourcesPath() / "Textures/nanosuit" / diffuseMapFilename, 4, FlipAxis::Y)
                    .unwrap(),
                VK_FORMAT_R8G8B8A8_SRGB));
        m_resourceContext->imageCache.addImageWithView(
            specularMapKey,
            createVulkanImage(
                *m_renderer,
                loadImage(m_renderer->getResourcesPath() / "Textures/nanosuit" / specularMapFilename, 4, FlipAxis::Y)
                    .unwrap(),
                VK_FORMAT_R8G8B8A8_UNORM));

        Material* partMaterial = m_resourceContext->createMaterial("normalMapPart" + std::to_string(i), pipeline);
        partMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        partMaterial->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));
        partMaterial->writeDescriptor(
            1,
            0,
            m_resourceContext->imageCache.getImageView(normalMapKey),
            m_resourceContext->imageCache.getSampler("linearRepeat"));
        partMaterial->writeDescriptor(
            1,
            1,
            m_resourceContext->imageCache.getImageView(diffuseMapKey),
            m_resourceContext->imageCache.getSampler("linearRepeat"));
        partMaterial->writeDescriptor(
            1,
            2,
            m_resourceContext->imageCache.getImageView(specularMapKey),
            m_resourceContext->imageCache.getSampler("linearRepeat"));

        nanosuit->pass(i, MainPass).material = partMaterial;
    }

    nanosuit->pass(0, MainPass).material = m_resourceContext->getMaterial("normalMapPart3");

    for (int i = 0; i < 100; ++i) {
        for (int j = 0; j < 100; ++j) {
            int idx = i * 100 + j + 2;
            auto smallNano = createRenderNode("nanosuit" + std::to_string(idx), idx);
            smallNano->geometry = m_resourceContext->getGeometry("nanosuit");
            smallNano->setModelMatrix(
                glm::translate(glm::vec3(-10.0f + j * 0.2f, 0.0f, -10.0f + i * 0.2f)) * glm::scale(glm::vec3(0.2f)));
            for (int k = 1; k < mesh.getViews().size(); ++k) {
                smallNano->pass(k, MainPass).material = m_resourceContext->getMaterial(
                    "normalMapP"
                    "art" +
                    std::to_string(k));
            }
        }
    }

    m_renderer->getDevice().flushDescriptorUpdates();
}

void NormalMappingScene::setupInput() {
    m_window->keyPressed += [this](Key key, int) {
        switch (key) {
        case Key::F5:
            m_resourceContext->recreatePipelines();
            break;
        case Key::Space:
            animationFrozen = !animationFrozen;
        }
    };
}

void NormalMappingScene::createGui() {
    using namespace gui;
    Form* form = nullptr; // m_app->getForm();

    auto panel = std::make_unique<Panel>(form);
    panel->setSizeHint({200.0f, 200.0f});
    panel->setPosition({-200.0f, 40.0f});

    auto fadeIn = std::make_shared<PropertyAnimation<float, Easing::Linear>>(1.0, 0.0f, 1.0f, 3.0);
    fadeIn->setUpdater([p = panel.get()](float v) { p->setOpacity(v); });
    form->getAnimator()->add(fadeIn, panel.get());

    auto slideIn = std::make_shared<PropertyAnimation<float, Easing::SlowOut>>(1.0, -200.0f, 20.0f, 3.0);
    slideIn->setUpdater([p = panel.get()](float v) { p->setPositionX(v); });
    form->getAnimator()->add(slideIn, panel.get());

    form->getAnimator()->clearObjectAnimations(panel.get());

    form->add(std::move(panel));
}
} // namespace crisp