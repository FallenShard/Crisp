#include "NormalMappingScene.hpp"

#include "Core/LuaConfig.hpp"
#include "Core/Application.hpp"
#include "Core/Window.hpp"

#include "Camera/CameraController.hpp"

#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "Vulkan/VulkanSampler.hpp"
#include "Vulkan/VulkanPipeline.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/Material.hpp"
#include "Renderer/RenderGraph.hpp"
#include "Renderer/VulkanBufferUtils.hpp"
#include "Renderer/VulkanImageUtils.hpp"
#include "Renderer/ResourceContext.hpp"
#include "Renderer/RenderPasses/ShadowPass.hpp"
#include "Renderer/RenderPasses/ForwardLightingPass.hpp"

#include "Models/Skybox.hpp"
#include "Geometry/TriangleMesh.hpp"
#include "Geometry/Geometry.hpp"
#include "Geometry/MeshGenerators.hpp"
#include "Geometry/TransformBuffer.hpp"
#include "Geometry/VertexLayout.hpp"

#include "Lights/LightSystem.hpp"
#include "Lights/DirectionalLight.hpp"
#include "Lights/EnvironmentLighting.hpp"

#include "GUI/Form.hpp"
#include "GUI/Label.hpp"
#include "GUI/CheckBox.hpp"
#include "GUI/Button.hpp"
#include "GUI/ComboBox.hpp"
#include "GUI/Slider.hpp"
#include "GUI/Panel.hpp"

#include <CrispCore/Math/Constants.hpp>
#include <CrispCore/Profiler.hpp>

#include <thread>

namespace crisp
{
    namespace
    {
        static constexpr uint32_t ShadowMapSize = 2048;
        static constexpr uint32_t CascadeCount = 4;

        static constexpr const char* MainPass = "mainPass";
        static constexpr const char* CsmPass = "csmPass";

        bool animationFrozen = true;
    }

    NormalMappingScene::NormalMappingScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_resourceContext(std::make_unique<ResourceContext>(renderer))
    {
        setupInput();

        // Camera
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);
        //m_cameraController->getCamera().setPosition(glm::vec3(5.0f, 5.0f, 5.0f));
        //m_cameraController->getCamera().setTarget(glm::vec3(0.0f));

        m_renderGraph = std::make_unique<RenderGraph>(m_renderer);

        // Main render pass
        m_renderGraph->addRenderPass(MainPass, std::make_unique<ForwardLightingPass>(m_renderer, VK_SAMPLE_COUNT_1_BIT));

        // Wrap-up render graph definition
        m_renderGraph->addRenderTargetLayoutTransition(MainPass, "SCREEN", 0);
        m_renderGraph->sortRenderPasses();
        m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);

        m_lightSystem = std::make_unique<LightSystem>(m_renderer, ShadowMapSize);
        m_lightSystem->setDirectionalLight(DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)));

        // Object transforms
        m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 10002);

        createPlane();


        m_renderer->getDevice()->flushDescriptorUpdates();

        createGui();
    }

    NormalMappingScene::~NormalMappingScene()
    {
        m_renderer->setSceneImageView(nullptr, 0);
        m_app->getForm()->remove("normalMappingPanel");
    }

    void NormalMappingScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);
    }

    void NormalMappingScene::update(float dt)
    {
        m_cameraController->update(dt);
        const auto& V = m_cameraController->getViewMatrix();
        const auto& P = m_cameraController->getProjectionMatrix();

        static float angle = 0.0f;
        if (!animationFrozen)
            angle += dt;

        m_renderNodeList[m_renderNodeMap["nanosuit"]]->transformPack->M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f));

        m_transformBuffer->update(V, P);

        m_lightSystem->update(m_cameraController->getCamera(), dt);

        m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));
    }

    void NormalMappingScene::render()
    {
        m_renderGraph->clearCommandLists();
        m_renderGraph->buildCommandLists(m_renderNodeList);
        m_renderGraph->executeCommandLists();
    }

    RenderNode* NormalMappingScene::createRenderNode(std::string id, int transformIndex)
    {
        //auto node = std::make_unique<RenderNode>(*m_transformBuffer, transformIndex);
        //m_renderNodes.emplace(id, std::move(node));
        //return m_renderNodes.at(id).get();
        m_renderNodeList.push_back(std::make_unique<RenderNode>(*m_transformBuffer, transformIndex));
        m_renderNodeMap[id] = m_renderNodeList.size() - 1;
        return m_renderNodeList.back().get();
    }

    void NormalMappingScene::createPlane()
    {
        std::vector<VertexAttributeDescriptor> pbrVertexFormat = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent };
        m_resourceContext->addGeometry("floor", std::make_unique<Geometry>(m_renderer, createPlaneMesh(pbrVertexFormat, 10.0f)));

        m_resourceContext->addSampler("linearRepeat", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f));
        m_resourceContext->addImageWithView("normalMap", createTexture(m_renderer, "hex-stones1-normal-dx.png", VK_FORMAT_R8G8B8A8_UNORM, false));
        m_resourceContext->addImageWithView("diffuseMap", createTexture(m_renderer, "hex-stones1-albedo.png", VK_FORMAT_R8G8B8A8_SRGB, false));
        m_resourceContext->addImageWithView("specularMap", createTexture(m_renderer, "hex-stones1-metallic.png", VK_FORMAT_R8G8B8A8_UNORM, false));

        VulkanPipeline* pipeline = m_resourceContext->createPipeline("normalMap", "NormalMap.lua", m_renderGraph->getRenderPass(MainPass), 0);
        Material* material = m_resourceContext->createMaterial("normalMap", pipeline);
        material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        material->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));
        material->writeDescriptor(1, 0, *m_resourceContext->getImageView("normalMap"), m_resourceContext->getSampler("linearRepeat"));
        material->writeDescriptor(1, 1, *m_resourceContext->getImageView("diffuseMap"), m_resourceContext->getSampler("linearRepeat"));
        material->writeDescriptor(1, 2, *m_resourceContext->getImageView("specularMap"), m_resourceContext->getSampler("linearRepeat"));

        auto floor = createRenderNode("floor", 0);
        floor->transformPack->M = glm::rotate(glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(1.0, 1.0f, 1.0f));
        floor->geometry = m_resourceContext->getGeometry("floor");
        floor->pass(MainPass).material = material;


        //m_resourceContext->addGeometry("sphere", std::make_unique<Geometry>(m_renderer, createPlaneMesh(pbrVertexFormat, 10.0f)));
        //
        //auto sphere = createRenderNode("sphere", 1);
        //sphere->transformPack->M = glm::translate(glm::vec3(0.0f, 1.0f, 0.0f));
        //sphere->geometry = m_resourceContext->getGeometry("sphere");

        // NANOSUIT
        TriangleMesh mesh(m_renderer->getResourcesPath() / "Meshes/nanosuit/nanosuit.obj", pbrVertexFormat);
        mesh.normalizeToUnitBox();
        mesh.transform(glm::translate(glm::vec3(0.0f, 0.5f, 0.0f)));
        m_resourceContext->addGeometry("nanosuit", std::make_unique<Geometry>(m_renderer, mesh));

        // Leg, Hand, Glass, Arm, Helmet, Body
        auto nanosuit = createRenderNode("nanosuit", 1);
        //nanosuit->transformPack->M = glm::rotate(glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(5.0f));
        nanosuit->geometry = m_resourceContext->getGeometry("nanosuit");
        //nanosuit->pass(MainPass).material = material;

        std::vector<std::string> parts = { "leg", "hand", "glass", "arm", "helmet", "body" };
        for (int i = 1; i < mesh.getGeometryParts().size(); ++i)
        {
            std::string normalMapKey = "partNormalMap" + std::to_string(i);
            std::string diffuseMapKey = "partDiffuseMap" + std::to_string(i);
            std::string specularMapKey = "partSpecularMap" + std::to_string(i);

            std::string normalMapFilename = parts[i - 1] + "_showroom_ddn.png";
            std::string diffuseMapFilename = parts[i - 1] + "_dif.png";
            std::string specularMapFilename = parts[i - 1] + "_showroom_spec.png";

            m_resourceContext->addImageWithView(normalMapKey, createTexture(m_renderer, "nanosuit/" + normalMapFilename,  VK_FORMAT_R8G8B8A8_SRGB, true));
            m_resourceContext->addImageWithView(diffuseMapKey, createTexture(m_renderer, "nanosuit/" + diffuseMapFilename, VK_FORMAT_R8G8B8A8_SRGB, true));
            m_resourceContext->addImageWithView(specularMapKey, createTexture(m_renderer, "nanosuit/" + specularMapFilename, VK_FORMAT_R8G8B8A8_SRGB, true));

            Material* material = m_resourceContext->createMaterial("normalMapPart" + std::to_string(i), pipeline);
            material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
            material->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));
            material->writeDescriptor(1, 0, *m_resourceContext->getImageView(normalMapKey), m_resourceContext->getSampler("linearRepeat"));
            material->writeDescriptor(1, 1, *m_resourceContext->getImageView(diffuseMapKey), m_resourceContext->getSampler("linearRepeat"));
            material->writeDescriptor(1, 2, *m_resourceContext->getImageView(specularMapKey), m_resourceContext->getSampler("linearRepeat"));

            nanosuit->pass(i, MainPass).material = material;
        }

        nanosuit->pass(0, MainPass).material = m_resourceContext->getMaterial("normalMapPart3");

        for (int i = 0; i < 100; ++i)
        {
            for (int j = 0; j < 100; ++j)
            {
                int idx = i * 100 + j + 2;
                auto smallNano = createRenderNode("nanosuit" + std::to_string(idx), idx);
                smallNano->geometry = m_resourceContext->getGeometry("nanosuit");
                smallNano->setModelMatrix(glm::translate(glm::vec3(-10.0f + j * 0.2f, 0.0f, -10.0f + i * 0.2f)) * glm::scale(glm::vec3(0.2f)));
                for (int k = 1; k < mesh.getGeometryParts().size(); ++k)
                    smallNano->pass(k, MainPass).material = m_resourceContext->getMaterial("normalMapPart" + std::to_string(k));
            }
        }

        m_renderer->getDevice()->flushDescriptorUpdates();
    }

    void NormalMappingScene::setupInput()
    {
        m_app->getWindow()->keyPressed += [this](Key key, int)
        {
            switch (key)
            {
            case Key::F5:
                m_resourceContext->recreatePipelines();
                break;
            case Key::Space:
                animationFrozen = !animationFrozen;
            }
        };

        m_app->getWindow()->mouseWheelScrolled += [this](double delta)
        {
            m_cameraController->updateFov(delta < 0 ? +5.0f : -5.0f);
        };
    }

    void NormalMappingScene::createGui()
    {
        using namespace gui;
        Form* form = m_app->getForm();

        auto panel = std::make_unique<Panel>(form);

        panel->setSizeHint({ 200.0f, 200.0f });
        panel->setPosition({ -200.0f, 40.0f });

        auto fadeIn = std::make_shared<PropertyAnimation<float, Easing::Linear>>(1.0, 0.0f, 1.0f);
        fadeIn->setStartDelay(3.0);
        fadeIn->setUpdater([p = panel.get()](float v){
            p->setOpacity(v);
        });
        form->getAnimator()->add(fadeIn, panel.get());

        auto slideIn = std::make_shared<PropertyAnimation<float, Easing::SlowOut>>(1.0, -200.0f, 20.0f);
        slideIn->setStartDelay(3.0);
        slideIn->setUpdater([p = panel.get()](float v){
            p->setPositionX(v);
        });
        form->getAnimator()->add(slideIn, panel.get());

        form->getAnimator()->clearObjectAnimations(panel.get());

        form->add(std::move(panel));

    }
}