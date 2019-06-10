#include "Scenes/TerrainScene.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "IO/ImageFileBuffer.hpp"
#include "Camera/CameraController.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph.hpp"
#include "Renderer/Material.hpp"

#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Renderer/Pipelines/TerrainPipeline.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/VulkanImageUtils.hpp"

#include "Geometry/Geometry.hpp"

#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanSampler.hpp"
#include "vulkan/VulkanDevice.hpp"

namespace crisp
{
    TerrainScene::TerrainScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_numTiles(128)
    {
        // Camera
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        // Main render pass
        auto mainRenderPass = std::make_unique<SceneRenderPass>(m_renderer);
        m_renderer->setSceneImageView(mainRenderPass.get(), 0);

        m_linearClampSampler = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        std::unique_ptr<ImageFileBuffer> image = std::make_unique<ImageFileBuffer>(renderer->getResourcesPath() / "Textures/heightmap.jpg", 1);

        auto heightMap = createTexture(m_renderer, *image, VK_FORMAT_R8_UNORM);
        auto heightMapView = heightMap->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1);

        m_transforms.M = glm::translate(glm::vec3(-m_numTiles / 2.0f, 0.0f, -m_numTiles / 2.0f));
        m_transformBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        int numVertsPerDim = m_numTiles + 1;
        std::vector<glm::vec3> data;
        for (int i = 0; i < numVertsPerDim; i++)
            for (int j = 0; j < numVertsPerDim; j++)
                data.push_back(glm::vec3(j, 0, i));

        std::vector<glm::uvec4> indexData;
        for (int i = 0; i < m_numTiles; i++)
            for (int j = 0; j < m_numTiles; j++)
                indexData.push_back(glm::uvec4(i * numVertsPerDim + j, (i + 1) * numVertsPerDim + j, (i + 1) * numVertsPerDim + j + 1, i * numVertsPerDim + j + 1));

        auto patchGeometry = std::make_unique<Geometry>(m_renderer, data, indexData);

        auto pipeline = createTerrainPipeline(m_renderer, mainRenderPass.get());
        auto material = std::make_unique<Material>(pipeline.get());
        material->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        material->writeDescriptor(0, 1, 0, heightMapView->getDescriptorInfo(m_linearClampSampler->getHandle()));
        material->writeDescriptor(0, 2, 0, m_cameraBuffer->getDescriptorInfo());
        material->addDynamicBufferInfo(*m_cameraBuffer, 0);
        m_renderer->getDevice()->flushDescriptorUpdates();

        m_images.push_back(std::move(heightMap));
        m_imageViews.push_back(std::move(heightMapView));

        m_geometries.push_back(std::move(patchGeometry));
        m_pipelines.push_back(std::move(pipeline));
        m_materials.push_back(std::move(material));


        m_renderGraph = std::make_unique<RenderGraph>(m_renderer);
        m_renderGraph->addRenderPass("scenePass", std::move(mainRenderPass));
        m_renderGraph->addRenderTargetLayoutTransition("scenePass", "SCREEN", 0);
        m_renderGraph->sortRenderPasses();
    }

    TerrainScene::~TerrainScene()
    {
    }

    void TerrainScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode("scenePass").renderPass.get(), 0);
    }

    void TerrainScene::update(float dt)
    {
        m_cameraController->update(dt);
        m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

        m_transforms.MV  = m_cameraController->getViewMatrix() * m_transforms.M;
        m_transforms.MVP = m_cameraController->getProjectionMatrix() * m_transforms.MV;
        m_transforms.N   = glm::inverse(glm::transpose(m_transforms.MV));
        m_transformBuffer->updateStagingBuffer(m_transforms);
    }

    void TerrainScene::render()
    {
        m_renderGraph->clearCommandLists();

        DrawCommand dc;
        dc.pipeline = m_materials[0]->getPipeline();
        dc.material = m_materials[0].get();
        dc.dynamicBuffers.push_back({ *m_transformBuffer, 0 * sizeof(TransformPack) });
        for (const auto& info : dc.material->getDynamicBufferInfos())
            dc.dynamicBuffers.push_back(info);
        dc.setPushConstants(1.0f, 15.0f);
        dc.geometry = m_geometries[0].get();
        dc.setGeometryView(dc.geometry->createIndexedGeometryView());

        auto& scenePass = m_renderGraph->getNode("scenePass");
        scenePass.addCommand(std::move(dc));

        m_renderGraph->executeDrawCommands();
    }
}