#include "ClusteredLightingScene.hpp"

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
#include "Renderer/RenderPasses/SceneRenderPass.hpp"

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

#include <CrispCore/Math/Constants.hpp>
#include <CrispCore/Profiler.hpp>

namespace crisp
{
    namespace
    {
        static constexpr uint32_t ShadowMapSize = 2048;
        static constexpr uint32_t CascadeCount = 4;

        static constexpr const char* MainPass = "mainPass";
        static constexpr const char* CsmPass = "csmPass";
    }

    ClusteredLightingScene::ClusteredLightingScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_resourceContext(std::make_unique<ResourceContext>(renderer))
    {
        setupInput();

        // Camera
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);
        //m_cameraController->getCamera().setPosition(glm::vec3(5.0f, 5.0f, 5.0f));

        m_renderGraph = std::make_unique<RenderGraph>(m_renderer);

        // Main render pass
        m_renderGraph->addRenderPass(MainPass, std::make_unique<SceneRenderPass>(m_renderer, VK_SAMPLE_COUNT_8_BIT));

        // Shadow map pass
        //m_renderGraph->addRenderPass(CsmPass, std::make_unique<ShadowPass>(m_renderer, ShadowMapSize, CascadeCount));
        //m_renderGraph->addRenderTargetLayoutTransition(CsmPass, MainPass, 0, CascadeCount);

        // Wrap-up render graph definition
        m_renderGraph->addRenderTargetLayoutTransition(MainPass, "SCREEN", 2);

        m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 2);

        m_lightSystem = std::make_unique<LightSystem>(m_renderer, ShadowMapSize);
        m_lightSystem->setDirectionalLight(DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)));
        m_lightSystem->enableCascadedShadowMapping(CascadeCount, ShadowMapSize);
        m_lightSystem->createPointLightBuffer(1024);
        m_lightSystem->createTileGridBuffers(*m_cameraController->getCameraParameters());
        m_lightSystem->clusterLights(*m_renderGraph, *m_resourceContext->getUniformBuffer("camera"));

        m_renderGraph->sortRenderPasses();

        // Object transforms
        m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 100);

        // Geometry setup
        std::vector<VertexAttributeDescriptor> shadowVertexFormat = { VertexAttribute::Position };
        m_resourceContext->addGeometry("cubeRT", std::make_unique<Geometry>(m_renderer, createCubeMesh({ VertexAttribute::Position, VertexAttribute::Normal })));
        m_resourceContext->addGeometry("cubeShadow", std::make_unique<Geometry>(m_renderer, createCubeMesh(shadowVertexFormat)));
        m_resourceContext->addGeometry("sphereShadow", std::make_unique<Geometry>(m_renderer, createSphereMesh(shadowVertexFormat)));
        m_resourceContext->addGeometry("floorShadow", std::make_unique<Geometry>(m_renderer, createPlaneMesh(shadowVertexFormat)));

        createCommonTextures();

        //for (uint32_t i = 0; i < CascadeCount; ++i)
        //{
        //    std::string key = "cascadedShadowMap" + std::to_string(i);
        //    auto csmPipeline = m_resourceContext->createPipeline(key, "ShadowMap.lua", m_renderGraph->getRenderPass(CsmPass), i);
        //    auto csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
        //    csmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        //    csmMaterial->writeDescriptor(0, 1, *m_lightSystem->getCascadedDirectionalLightBuffer(i));
        //}

        createPlane();
        createShaderball();

        m_renderer->getDevice()->flushDescriptorUpdates();

        createGui(m_app->getForm());
    }

    ClusteredLightingScene::~ClusteredLightingScene()
    {
        m_renderer->setSceneImageView(nullptr, 0);
        m_app->getForm()->remove("shadowMappingPanel");
    }

    void ClusteredLightingScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 2);
    }

    void ClusteredLightingScene::update(float dt)
    {
        m_cameraController->update(dt);
        const auto& V = m_cameraController->getViewMatrix();
        const auto& P = m_cameraController->getProjectionMatrix();

        m_transformBuffer->update(V, P);

        m_lightSystem->update(m_cameraController->getCamera(), dt);

        m_resourceContext->getUniformBuffer("pbrUnifParams")->updateStagingBuffer(m_uniformMaterialParams);

        m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));
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
        auto node = std::make_unique<RenderNode>(*m_transformBuffer, transformIndex);
        m_renderNodes.emplace(id, std::move(node));
        return m_renderNodes.at(id).get();
    }

    void ClusteredLightingScene::createCommonTextures()
    {
        m_resourceContext->addSampler("nearestNeighbor", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
        m_resourceContext->addSampler("linearRepeat", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f));
        m_resourceContext->addSampler("linearMipmap", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 5.0f));
        m_resourceContext->addSampler("linearClamp", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 11.0f));
        m_resourceContext->addSampler("linearMirrorRepeat", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, 16.0f, 11.0f));

        // Environment map
        LuaConfig config(m_renderer->getResourcesPath() / "Scripts/scene.lua");
        auto hdrName = config.get<std::string>("environmentMap").value_or("GreenwichPark") + ".hdr";
        auto envRefMap = createEnvironmentMap(m_renderer, hdrName, VK_FORMAT_R32G32B32A32_SFLOAT, true);
        std::shared_ptr<VulkanImageView> envRefMapView = envRefMap->createView(VK_IMAGE_VIEW_TYPE_2D);

        auto [cubeMap, cubeMapView] = convertEquirectToCubeMap(m_renderer, envRefMapView, 1024);

        auto [diffEnv, diffEnvView] = setupDiffuseEnvMap(m_renderer, *cubeMapView, 512);
        m_resourceContext->addImageWithView("envIrrMap", std::move(diffEnv), std::move(diffEnvView));
        auto [reflEnv, reflEnvView] = setupReflectEnvMap(m_renderer, *cubeMapView, 1024);
        m_resourceContext->addImageWithView("filteredMap", std::move(reflEnv), std::move(reflEnvView));

        m_resourceContext->addImageWithView("cubeMap", std::move(cubeMap), std::move(cubeMapView));
        m_resourceContext->addImageWithView("brdfLut", integrateBrdfLut(m_renderer));

        auto pbrPipeline = m_resourceContext->createPipeline("pbrUnif", "Pbr.lua", m_renderGraph->getRenderPass(MainPass), 0);
        auto pbrMaterial = m_resourceContext->createMaterial("pbrUnif", pbrPipeline);
        pbrMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        pbrMaterial->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));

        m_uniformMaterialParams.albedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_uniformMaterialParams.metallic = 0.0f;
        m_uniformMaterialParams.roughness = 0.1f;
        m_resourceContext->addUniformBuffer("pbrUnifParams", std::make_unique<UniformBuffer>(m_renderer, m_uniformMaterialParams, BufferUpdatePolicy::PerFrame));
        pbrMaterial->writeDescriptor(0, 2, *m_resourceContext->getUniformBuffer("pbrUnifParams"));

        pbrMaterial->writeDescriptor(1, 0, *m_lightSystem->getPointLightBuffer());
        pbrMaterial->writeDescriptor(1, 1, *m_lightSystem->getLightIndexBuffer());

        pbrMaterial->writeDescriptor(2, 0, *m_resourceContext->getImageView("envIrrMap"), m_resourceContext->getSampler("linearClamp"));
        pbrMaterial->writeDescriptor(2, 1, *m_resourceContext->getImageView("filteredMap"), m_resourceContext->getSampler("linearMipmap"));
        pbrMaterial->writeDescriptor(2, 2, *m_resourceContext->getImageView("brdfLut"), m_resourceContext->getSampler("linearClamp"));

        pbrMaterial->writeDescriptor(3, 0, m_lightSystem->getTileGridViews(), nullptr, VK_IMAGE_LAYOUT_GENERAL);
    }

    void ClusteredLightingScene::createShaderball()
    {
        std::vector<VertexAttributeDescriptor> shadowVertexFormat = { VertexAttribute::Position };
        std::vector<VertexAttributeDescriptor> pbrVertexFormat = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent };

        TriangleMesh mesh(m_renderer->getResourcesPath() / "Meshes/sponza_fixed.obj", pbrVertexFormat);
        m_resourceContext->addGeometry("shaderBallPbr", std::make_unique<Geometry>(m_renderer, mesh, pbrVertexFormat));
        m_resourceContext->addGeometry("shaderBallShadow", std::make_unique<Geometry>(m_renderer, mesh, shadowVertexFormat));



        auto pbrShaderBall = createRenderNode("pbrShaderBall", 3);
        pbrShaderBall->transformPack->M = glm::scale(glm::vec3(5.f));
        pbrShaderBall->geometry = m_resourceContext->getGeometry("shaderBallPbr");
        pbrShaderBall->pass(MainPass).material = m_resourceContext->getMaterial("pbrUnif");

        //for (uint32_t i = 0; i < CascadeCount; ++i)
        //{
        //    auto& subpass = pbrShaderBall->subpass(CsmPass, i);
        //    subpass.geometry = m_resourceContext->getGeometry("shaderBallShadow");
        //    subpass.material = m_resourceContext->getMaterial("cascadedShadowMap" + std::to_string(i));
        //}

        m_renderer->getDevice()->flushDescriptorUpdates();
    }

    void ClusteredLightingScene::createPlane()
    {
        //std::vector<VertexAttributeDescriptor> pbrVertexFormat = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent };
        //
        //m_resourceContext->addGeometry("floor", std::make_unique<Geometry>(m_renderer, createPlaneMesh(pbrVertexFormat, 200.0f)));
        //
        //auto floor = createRenderNode("floor", 0);
        //floor->transformPack->M = glm::scale(glm::vec3(1.0, 1.0f, 1.0f));
        //floor->geometry = m_resourceContext->getGeometry("floor");
        //floor->pass(MainPass).material = m_resourceContext->getMaterial("pbrUnif");
        //floor->pass(MainPass).setPushConstants(glm::vec2(100.0f));
        //
        //m_renderer->getDevice()->flushDescriptorUpdates();
    }

    void ClusteredLightingScene::setupInput()
    {
        m_app->getWindow()->keyPressed += [this](Key key, int)
        {
            switch (key)
            {
            case Key::F5:
                m_resourceContext->recreatePipelines();
                break;
            }
        };

        m_app->getWindow()->mouseWheelScrolled += [this](double delta)
        {
            float fov = m_cameraController->getCamera().getFov();
            if (delta < 0)
                fov = std::min(90.0f, fov + 5.0f);
            else
                fov = std::max(5.0f, fov - 5.0f);
            m_cameraController->getCamera().setFov(fov);
        };
    }

    void ClusteredLightingScene::createGui(gui::Form* form)
    {
    }
}