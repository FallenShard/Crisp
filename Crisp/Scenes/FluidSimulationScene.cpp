#include "FluidSimulationScene.hpp"

#include "Models/FluidSimulation.hpp"
#include "Models/PositionBasedFluid.hpp"
#include "Models/SPH.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"
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
#include "Renderer/RenderGraph.hpp"

namespace crisp
{
    namespace
    {
        static constexpr const char* MainPass = "mainPass";
    }

    FluidSimulationScene::FluidSimulationScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
    {
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_uniformBuffers.emplace("camera", std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame));

        m_renderGraph = std::make_unique<RenderGraph>(m_renderer);
        auto& mainPassNode = m_renderGraph->addRenderPass(MainPass, std::make_unique<SceneRenderPass>(m_renderer));
        m_renderGraph->addRenderTargetLayoutTransition(MainPass, "SCREEN", 0);
        m_renderGraph->sortRenderPasses();
        m_renderer->setSceneImageView(mainPassNode.renderPass.get(), 0);

        m_fluidSimulation = std::make_unique<SPH>(m_renderer);
        m_app->getWindow()->keyPressed.subscribe<&FluidSimulation::onKeyPressed>(m_fluidSimulation.get());

        auto fluidPanel = std::make_unique<gui::FluidSimulationPanel>(app->getForm(), m_fluidSimulation.get());
        m_app->getForm()->add(std::move(fluidPanel));

        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(TransformPack), BufferUpdatePolicy::PerFrame);
        m_uniformBuffers.emplace("params", std::make_unique<UniformBuffer>(m_renderer, sizeof(ParticleParams), BufferUpdatePolicy::PerFrame));

        m_pointSpritePipeline = createPointSphereSpritePipeline(m_renderer, mainPassNode.renderPass.get());
        m_pointSpriteDescGroup =
        {
            m_pointSpritePipeline->allocateDescriptorSet(0),
            m_pointSpritePipeline->allocateDescriptorSet(1)
        };
        m_pointSpriteDescGroup.postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo());
        m_pointSpriteDescGroup.postBufferUpdate(1, 0, m_uniformBuffers.at("params")->getDescriptorInfo());
        m_pointSpriteDescGroup.flushUpdates(m_renderer->getDevice());
    }

    FluidSimulationScene::~FluidSimulationScene()
    {
        m_app->getWindow()->keyPressed.unsubscribe<&FluidSimulation::onKeyPressed>(m_fluidSimulation.get());
        m_app->getForm()->remove("fluidSimulationPanel");
    }

    void FluidSimulationScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);
    }

    void FluidSimulationScene::update(float dt)
    {
        m_cameraController->update(dt);
        m_uniformBuffers["camera"]->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

        m_transforms.M = glm::scale(glm::vec3(10.0f));
        m_transforms.MV = m_cameraController->getViewMatrix() * m_transforms.M;
        m_transforms.MVP = m_cameraController->getProjectionMatrix() * m_transforms.MV;
        m_transformsBuffer->updateStagingBuffer(m_transforms);

        m_particleParams.radius = m_fluidSimulation->getParticleRadius() * 10.0f;
        m_particleParams.screenSpaceScale = m_renderer->getSwapChainExtent().width * m_cameraController->getProjectionMatrix()[0][0];
        m_uniformBuffers["params"]->updateStagingBuffer(m_particleParams);

        m_fluidSimulation->update(dt);
    }

    void FluidSimulationScene::render()
    {
        //m_renderGraph->clearCommandLists();
        //m_renderGraph->buildCommandLists();
        //m_renderGraph->executeCommandLists();

        m_renderer->enqueueDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            auto& passNode = m_renderGraph->getNode(MainPass);

            m_fluidSimulation->dispatchCompute(commandBuffer, frameIdx);
            passNode.renderPass->begin(commandBuffer);
            m_pointSpriteDescGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx));
            m_pointSpriteDescGroup.setDynamicOffset(1, m_uniformBuffers["params"]->getDynamicOffset(frameIdx));
            m_pointSpritePipeline->bind(commandBuffer);
            m_renderer->setDefaultViewport(commandBuffer);
            m_renderer->setDefaultScissor(commandBuffer);
            m_pointSpriteDescGroup.bind(commandBuffer, m_pointSpritePipeline->getPipelineLayout()->getHandle());
            m_fluidSimulation->drawGeometry(commandBuffer);
            passNode.renderPass->end(commandBuffer, frameIdx);

            passNode.renderPass->getRenderTarget(0)->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIdx, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
    }
}