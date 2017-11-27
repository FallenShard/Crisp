#include "FluidSimulationScene.hpp"

#include "Models/FluidSimulation.hpp"


#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "Core/InputDispatcher.hpp"
#include "Camera/CameraController.hpp"

#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Renderer/TextureView.hpp"
#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "vulkan/VulkanSampler.hpp"

#include "GUI/Form.hpp"
#include "FluidSimulationPanel.hpp"

namespace crisp
{
    FluidSimulationScene::FluidSimulationScene(VulkanRenderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_device(m_renderer->getDevice())
    {
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        m_scenePass = std::make_unique<SceneRenderPass>(m_renderer);
        initRenderTargetResources();

        m_fluidSimulation = std::make_unique<FluidSimulation>(m_renderer, m_scenePass.get());
        m_app->getWindow()->getInputDispatcher()->keyPressed.subscribe<FluidSimulation, &FluidSimulation::onKeyPressed>(m_fluidSimulation.get());

        auto fluidPanel = std::make_unique<gui::FluidSimulationPanel>(app->getForm(), m_fluidSimulation.get());
        m_app->getForm()->add(std::move(fluidPanel));
    }

    FluidSimulationScene::~FluidSimulationScene()
    {
        m_app->getWindow()->getInputDispatcher()->keyPressed.unsubscribe<FluidSimulation, &FluidSimulation::onKeyPressed>(m_fluidSimulation.get());
        m_app->getForm()->remove("fluidSimulationPanel");
    }

    void FluidSimulationScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_scenePass->recreate();

        m_sceneImageView = m_scenePass->createRenderTargetView(SceneRenderPass::Opaque);
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_sceneImageView->getDescriptorInfo());
        m_sceneDescSetGroup.flushUpdates(m_device);
    }

    void FluidSimulationScene::update(float dt)
    {
        auto viewChanged = m_cameraController->update(dt);
        if (viewChanged)
            m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

        m_fluidSimulation->update(m_cameraController->getViewMatrix(), m_cameraController->getProjectionMatrix(), dt);
    }

    void FluidSimulationScene::render()
    {
        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();
            m_cameraBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            m_fluidSimulation->updateDeviceBuffers(commandBuffer, frameIdx);
        });

        m_renderer->enqueueDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            m_scenePass->begin(commandBuffer);
            m_fluidSimulation->draw(commandBuffer, frameIdx);
            m_scenePass->end(commandBuffer);

            VkImageMemoryBarrier imageMemoryBarrier = {};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.image = m_scenePass->getColorAttachment();
            imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
            imageMemoryBarrier.subresourceRange.levelCount = 1;
            imageMemoryBarrier.subresourceRange.baseArrayLayer = frameIdx;
            imageMemoryBarrier.subresourceRange.layerCount = 1;
            imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        });

        m_renderer->enqueueDefaultPassDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            m_fsQuadPipeline->bind(commandBuffer);
            m_renderer->setDefaultViewport(commandBuffer);
            m_sceneDescSetGroup.bind(commandBuffer, m_fsQuadPipeline->getPipelineLayout());

            unsigned int layerIndex = m_renderer->getCurrentVirtualFrameIndex();
            m_fsQuadPipeline->setPushConstant(commandBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, 0, layerIndex);
            m_renderer->drawFullScreenQuad(commandBuffer);
        });
    }

    void FluidSimulationScene::initRenderTargetResources()
    {
        m_linearClampSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        m_fsQuadPipeline = std::make_unique<FullScreenQuadPipeline>(m_renderer, m_renderer->getDefaultRenderPass(), true);
        m_sceneDescSetGroup = { m_fsQuadPipeline->allocateDescriptorSet(FullScreenQuadPipeline::DisplayedImage) };

        m_sceneImageView = m_scenePass->createRenderTargetView(SceneRenderPass::Opaque);
        m_sceneDescSetGroup.postImageUpdate(0, 0, VK_DESCRIPTOR_TYPE_SAMPLER, m_sceneImageView->getDescriptorInfo(m_linearClampSampler->getHandle()));
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_sceneImageView->getDescriptorInfo());
        m_sceneDescSetGroup.flushUpdates(m_device);
    }
}