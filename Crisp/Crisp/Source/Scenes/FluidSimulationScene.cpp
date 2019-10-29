#include "FluidSimulationScene.hpp"

#include "Models/FluidSimulation.hpp"
#include "Models/PositionBasedFluid.hpp"
#include "Models/SPH.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "Core/EventHub.hpp"
#include "Camera/CameraController.hpp"

#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/Pipelines/PointSphereSpritePipeline.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanImage.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "vulkan/VulkanSampler.hpp"

#include "GUI/Form.hpp"
#include "FluidSimulationPanel.hpp"

namespace crisp
{
    FluidSimulationScene::FluidSimulationScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_device(m_renderer->getDevice())
    {
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        m_scenePass = std::make_unique<SceneRenderPass>(m_renderer);
        m_renderer->setSceneImageView(m_scenePass.get(), 0);

        m_fluidSimulation = std::make_unique<SPH>(m_renderer);
        m_app->getWindow()->getEventHub().keyPressed.subscribe<&FluidSimulation::onKeyPressed>(m_fluidSimulation.get());

        auto fluidPanel = std::make_unique<gui::FluidSimulationPanel>(app->getForm(), m_fluidSimulation.get());
        m_app->getForm()->add(std::move(fluidPanel));

        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(TransformPack), BufferUpdatePolicy::PerFrame);
        m_paramsBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(ParticleParams), BufferUpdatePolicy::PerFrame);

        m_pointSpritePipeline = createPointSphereSpritePipeline(m_renderer, m_scenePass.get());
        m_pointSpriteDescGroup =
        {
            m_pointSpritePipeline->allocateDescriptorSet(0),
            m_pointSpritePipeline->allocateDescriptorSet(1)
        };
        m_pointSpriteDescGroup.postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo());
        m_pointSpriteDescGroup.postBufferUpdate(1, 0, m_paramsBuffer->getDescriptorInfo());
        m_pointSpriteDescGroup.flushUpdates(m_device);
    }

    FluidSimulationScene::~FluidSimulationScene()
    {
        m_app->getWindow()->getEventHub().keyPressed.unsubscribe<&FluidSimulation::onKeyPressed>(m_fluidSimulation.get());
        m_app->getForm()->remove("fluidSimulationPanel");
    }

    void FluidSimulationScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_scenePass->recreate();
        m_renderer->setSceneImageView(m_scenePass.get(), 0);
    }

    void FluidSimulationScene::update(float dt)
    {
        auto viewChanged = m_cameraController->update(dt);
        if (viewChanged)
            m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

        m_transforms.M = glm::scale(glm::vec3(10.0f));
        m_transforms.MV = m_cameraController->getViewMatrix() * m_transforms.M;
        m_transforms.MVP = m_cameraController->getProjectionMatrix() * m_transforms.MV;
        m_transformsBuffer->updateStagingBuffer(m_transforms);

        m_particleParams.radius = m_fluidSimulation->getParticleRadius() * 10.0f;
        m_particleParams.screenSpaceScale = m_renderer->getSwapChainExtent().width * m_cameraController->getProjectionMatrix()[0][0];
        m_paramsBuffer->updateStagingBuffer(m_particleParams);

        m_fluidSimulation->update(dt);
    }

    void FluidSimulationScene::render()
    {
        m_renderer->enqueueDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            m_fluidSimulation->dispatchCompute(commandBuffer, frameIdx);
            m_scenePass->begin(commandBuffer);
            m_pointSpriteDescGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx));
            m_pointSpriteDescGroup.setDynamicOffset(1, m_paramsBuffer->getDynamicOffset(frameIdx));
            m_pointSpritePipeline->bind(commandBuffer);
            m_renderer->setDefaultViewport(commandBuffer);
            m_renderer->setDefaultScissor(commandBuffer);
            m_pointSpriteDescGroup.bind(commandBuffer, m_pointSpritePipeline->getPipelineLayout()->getHandle());
            m_fluidSimulation->drawGeometry(commandBuffer);
            m_scenePass->end(commandBuffer, frameIdx);

            m_scenePass->getRenderTarget(0)->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIdx, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
    }
}