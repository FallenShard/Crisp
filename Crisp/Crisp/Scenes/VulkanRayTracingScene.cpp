#include "VulkanRayTracingScene.hpp"

#include <CrispCore/LuaConfig.hpp>
#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>

#include <Crisp/Camera/FreeCameraController.hpp>

#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanSampler.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/VulkanBufferUtils.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>

#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Geometry/MeshGenerators.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>

#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Lights/DirectionalLight.hpp>
#include <Crisp/Lights/EnvironmentLighting.hpp>

#include <Crisp/GUI/Form.hpp>
#include <Crisp/GUI/Label.hpp>
#include <Crisp/GUI/CheckBox.hpp>
#include <Crisp/GUI/Button.hpp>
#include <Crisp/GUI/ComboBox.hpp>
#include <Crisp/GUI/Slider.hpp>

#include <CrispCore/Math/Constants.hpp>
#include <CrispCore/Profiler.hpp>

#include <Crisp/Vulkan/VulkanRayTracer.hpp>
#include <Crisp/Vulkan/RayTracingMaterial.hpp>

#include <CrispCore/IO/MeshReader.hpp>

#include <random>

namespace crisp
{
    namespace
    {
        static constexpr uint32_t ShadowMapSize = 2048;
        static constexpr uint32_t CascadeCount = 4;

        static constexpr const char* MainPass = "mainPass";
        static constexpr const char* CsmPass = "csmPass";
    }

    VulkanRayTracingScene::VulkanRayTracingScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_resourceContext(std::make_unique<ResourceContext>(renderer))
    {
        setupInput();

        // Camera
        m_cameraController = std::make_unique<FreeCameraController>(app->getWindow());
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
        m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 100);

        createPlane();


        std::vector<VertexAttributeDescriptor> posFormat = { VertexAttribute::Position, VertexAttribute::Normal };
        m_resourceContext->addGeometry("floorPos", std::make_unique<Geometry>(m_renderer, createPlaneMesh(posFormat, 10.0f)));
        m_resourceContext->addGeometry("walls", std::make_unique<Geometry>(m_renderer, loadMesh(m_renderer->getResourcesPath() / "Meshes/walls.obj", posFormat), posFormat, true, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
        m_resourceContext->addGeometry("leftwall", std::make_unique<Geometry>(m_renderer, loadMesh(m_renderer->getResourcesPath() / "Meshes/leftwall.obj", posFormat), posFormat, true, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
        m_resourceContext->addGeometry("rightwall", std::make_unique<Geometry>(m_renderer, loadMesh(m_renderer->getResourcesPath() / "Meshes/rightwall.obj", posFormat), posFormat, true, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));

        TriangleMesh lightMesh(loadMesh(m_renderer->getResourcesPath() / "Meshes/light.obj", posFormat));
        //lightMesh.transform(glm::translate(glm::vec3(0.0f, -0.2f, 0.0f)));
        //TriangleMesh lightMesh(m_renderer->getResourcesPath() / "Meshes/sphere.obj", posFormat);
        //lightMesh.transform(glm::translate(glm::vec3(0.0f, +0.8f, 0.0f)) * glm::scale(glm::vec3(0.1f)));
        m_resourceContext->addGeometry("light", std::make_unique<Geometry>(m_renderer, lightMesh, posFormat, true, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));

        TriangleMesh leftSphere(loadMesh(m_renderer->getResourcesPath() / "Meshes/sphere.obj", posFormat));
        leftSphere.transform(glm::translate(glm::vec3(-0.421400, 0.332100, -0.280000)) * glm::scale(glm::vec3(0.3263)));
        m_resourceContext->addGeometry("leftSphere", std::make_unique<Geometry>(m_renderer, leftSphere, posFormat, true, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
        TriangleMesh rightSphere(loadMesh(m_renderer->getResourcesPath() / "Meshes/Ocean3.obj", posFormat));
        //rightSphere.transform(glm::translate(glm::vec3(0.445800, 0.332100, 0.376700)) * glm::scale(glm::vec3(0.3263)));
        rightSphere.transform(glm::translate(glm::vec3(0.000, 0.232100, 0.000)) * glm::scale(glm::vec3(0.5f)));// *glm::scale(glm::vec3(0.3263)));
        m_resourceContext->addGeometry("rightSphere", std::make_unique<Geometry>(m_renderer, rightSphere, posFormat, true, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));


        m_rayTracer = std::make_unique<VulkanRayTracer>(m_renderer->getDevice());
        m_rayTracer->addTriangleGeometry(*m_resourceContext->getGeometry("walls"), glm::mat4(1.0f));
        m_rayTracer->addTriangleGeometry(*m_resourceContext->getGeometry("leftwall"), glm::mat4(1.0f));
        m_rayTracer->addTriangleGeometry(*m_resourceContext->getGeometry("rightwall"), glm::mat4(1.0f));
        m_rayTracer->addTriangleGeometry(*m_resourceContext->getGeometry("light"), glm::mat4(1.0f));
        m_rayTracer->addTriangleGeometry(*m_resourceContext->getGeometry("leftSphere"), glm::mat4(1.0f));
        m_rayTracer->addTriangleGeometry(*m_resourceContext->getGeometry("rightSphere"), glm::mat4(1.0f));

        std::default_random_engine eng;
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::vector<float> randData;
        for (int i = 0; i < 65536 / sizeof(float); ++i)
        {
            randData.push_back(dist(eng));
        }
        auto randBuffer = m_resourceContext->createUniformBuffer("random", randData, BufferUpdatePolicy::Constant);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
            {
                m_rayTracer->build(cmdBuffer);
            });
        m_renderer->flushResourceUpdates(true);
        m_rayTracer->createImage(m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height, Renderer::NumVirtualFrames);

        m_rayTracingMaterial = std::make_unique<RayTracingMaterial>(m_renderer, m_rayTracer->getTopLevelAccelerationStructure(),
            m_rayTracer->getImageViewHandles(), *m_resourceContext->getUniformBuffer("camera"));
        m_rayTracingMaterial->updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("walls"), 0);
        m_rayTracingMaterial->updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("leftwall"), 1);
        m_rayTracingMaterial->updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("rightwall"), 2);
        m_rayTracingMaterial->updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("light"), 3);
        m_rayTracingMaterial->updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("leftSphere"), 4);
        m_rayTracingMaterial->updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("rightSphere"), 5);
        m_rayTracingMaterial->setRandomBuffer(*m_resourceContext->getUniformBuffer("random"));

        m_renderer->getDevice()->flushDescriptorUpdates();

        m_renderer->setSceneImageViews(m_rayTracer->getImageViews());

        createGui();
    }

    VulkanRayTracingScene::~VulkanRayTracingScene()
    {
        m_renderer->setSceneImageView(nullptr, 0);
        m_app->getForm()->remove("normalMappingPanel");
    }

    void VulkanRayTracingScene::resize(int width, int height)
    {
        m_cameraController->onViewportResized(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);
    }

    void VulkanRayTracingScene::update(float dt)
    {
        m_cameraController->update(dt);
        m_rayTracingMaterial->resetFrameCounter();

        const CameraParameters cameraParams = m_cameraController->getCameraParameters();
        m_transformBuffer->update(cameraParams.V, cameraParams.P);

        // TODO
        //m_lightSystem->update(m_cameraController->getCamera(), dt);

        m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(cameraParams);
    }

    void VulkanRayTracingScene::render()
    {
        //m_renderGraph->clearCommandLists();
        //m_renderGraph->buildCommandLists(m_renderNodes);
        //m_renderGraph->executeCommandLists();

        m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer) {
            m_renderer->finish();
            m_rayTracer->traceRays(cmdBuffer, m_renderer->getCurrentVirtualFrameIndex(), *m_rayTracingMaterial);
            });
    }

    RenderNode* VulkanRayTracingScene::createRenderNode(std::string id, int transformIndex)
    {
        auto node = std::make_unique<RenderNode>(*m_transformBuffer, transformIndex);
        m_renderNodes.emplace(id, std::move(node));
        return m_renderNodes.at(id).get();
    }

    void VulkanRayTracingScene::createPlane()
    {
        std::vector<VertexAttributeDescriptor> pbrVertexFormat = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent };
        m_resourceContext->addGeometry("floor", std::make_unique<Geometry>(m_renderer, createPlaneMesh(pbrVertexFormat, 10.0f)));

        m_resourceContext->addSampler("linearRepeat", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f));
        m_resourceContext->addImageWithView("normalMap", createTexture(m_renderer, "hex-stones1-normal-dx.png", VK_FORMAT_R8G8B8A8_UNORM, false));
        m_resourceContext->addImageWithView("diffuseMap", createTexture(m_renderer, "hex-stones1-albedo.png", VK_FORMAT_R8G8B8A8_SRGB, false));

        VulkanPipeline* pipeline = m_resourceContext->createPipeline("normalMap", "NormalMap.lua", m_renderGraph->getRenderPass(MainPass), 0);
        Material* material = m_resourceContext->createMaterial("normalMap", pipeline);
        material->writeDescriptor(0, 0, *m_transformBuffer->getUniformBuffer());
        material->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));
        material->writeDescriptor(1, 0, *m_resourceContext->getImageView("normalMap"), m_resourceContext->getSampler("linearRepeat"));
        material->writeDescriptor(1, 1, *m_resourceContext->getImageView("diffuseMap"), m_resourceContext->getSampler("linearRepeat"));

        auto floor = createRenderNode("floor", 0);
        floor->transformPack->M = glm::rotate(glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(1.0, 1.0f, 1.0f));
        floor->geometry = m_resourceContext->getGeometry("floor");
        floor->pass(MainPass).material = material;



        m_renderer->getDevice()->flushDescriptorUpdates();
    }

    void VulkanRayTracingScene::setupInput()
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
    }

    void VulkanRayTracingScene::createGui()
    {
    }
}