#include "Scenes/TerrainScene.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "IO/ImageFileBuffer.hpp"
#include "Camera/CameraController.hpp"

#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Renderer/Pipelines/TerrainPipeline.hpp"
#include "vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanSampler.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/IndexBuffer.hpp"
#include "Renderer/Texture.hpp"
#include "Renderer/VertexBuffer.hpp"

namespace crisp
{
    TerrainScene::TerrainScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_device(m_renderer->getDevice())
        , m_numTiles(128)
    {
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        m_scenePass = std::make_unique<SceneRenderPass>(m_renderer);
        m_linearClampSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        std::unique_ptr<ImageFileBuffer> image = std::make_unique<ImageFileBuffer>(renderer->getResourcesPath() / "Textures/heightmap.jpg", 1);

        m_heightMap = std::make_unique<Texture>(m_renderer, VkExtent3D{ image->getWidth(), image->getHeight(), 1 }, 1, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_heightMap->fill(image->getData(), image->getByteSize());
        m_heightMapView = m_heightMap->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1);

        m_transforms.M = glm::translate(glm::vec3(-m_numTiles / 2.0f, 0.0f, -m_numTiles / 2.0f));
        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        int numVertsPerDim = m_numTiles + 1;
        std::vector<glm::vec3> data;
        for (int i = 0; i < numVertsPerDim; i++)
            for (int j = 0; j < numVertsPerDim; j++)
                data.push_back(glm::vec3(j, 0, i));

        std::vector<glm::uvec4> indexData;
        for (int i = 0; i < m_numTiles; i++)
            for (int j = 0; j < m_numTiles; j++)
                indexData.push_back(glm::uvec4(i * numVertsPerDim + j, (i + 1) * numVertsPerDim + j, (i + 1) * numVertsPerDim + j + 1, i * numVertsPerDim + j + 1));

        m_vertexBuffer = std::make_unique<VertexBuffer>(m_renderer, data);
        m_indexBuffer = std::make_unique<IndexBuffer>(m_renderer, indexData);

        m_terrainPipeline = createTerrainPipeline(m_renderer, m_scenePass.get());
        m_terrainDescGroup =
        {
            m_terrainPipeline->allocateDescriptorSet(0)
        };
        m_terrainDescGroup.postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo());
        m_terrainDescGroup.postImageUpdate(0, 1, m_heightMapView->getDescriptorInfo(m_linearClampSampler->getHandle()));
        m_terrainDescGroup.postBufferUpdate(0, 2, m_cameraBuffer->getDescriptorInfo());
        m_terrainDescGroup.flushUpdates(m_device);
    }

    TerrainScene::~TerrainScene()
    {
    }

    void TerrainScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_scenePass->recreate();
        m_renderer->setSceneImageView(m_scenePass->createRenderTargetView(0, Renderer::NumVirtualFrames));
    }

    void TerrainScene::update(float dt)
    {
        auto viewChanged = m_cameraController->update(dt);
        if (viewChanged)
            m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

        m_transforms.MV = m_cameraController->getViewMatrix() * m_transforms.M;
        m_transforms.MVP = m_cameraController->getProjectionMatrix() * m_transforms.MV;
        m_transforms.N = glm::inverse(glm::transpose(m_transforms.MV));
        m_transformsBuffer->updateStagingBuffer(m_transforms);
    }

    void TerrainScene::render()
    {
        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();
            m_cameraBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            m_transformsBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
        });

        m_renderer->enqueueDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            m_scenePass->begin(commandBuffer);

            m_terrainDescGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx));
            m_terrainDescGroup.setDynamicOffset(1, m_cameraBuffer->getDynamicOffset(frameIdx));
            m_terrainPipeline->bind(commandBuffer);
            m_terrainPipeline->setPushConstants(commandBuffer, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 1.0f, 15.0f);
            m_renderer->setDefaultViewport(commandBuffer);
            m_renderer->setDefaultScissor(commandBuffer);

            m_terrainDescGroup.bind(commandBuffer, m_terrainPipeline->getPipelineLayout()->getHandle());
            VkDeviceSize offset = 0;
            VkBuffer buf = m_vertexBuffer->get();
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &buf, &offset);
            m_indexBuffer->bind(commandBuffer, 0);
            vkCmdDrawIndexed(commandBuffer, 4 * m_numTiles * m_numTiles, 1, 0, 0, 0);

            m_scenePass->end(commandBuffer);

            m_scenePass->getRenderTarget(0)->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIdx, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
    }
}