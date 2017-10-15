#include "Scene.hpp"

#include "Application.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/Pipelines/UniformColorPipeline.hpp"
#include "Renderer/Pipelines/PointSphereSpritePipeline.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Renderer/Pipelines/LiquidPipeline.hpp"
#include "Renderer/Pipelines/BlinnPhongPipeline.hpp"
#include "Renderer/Pipelines/ShadowMapPipeline.hpp"
#include "Renderer/Pipelines/OutlinePipeline.hpp"
#include "Renderer/Pipelines/NormalPipeline.hpp"
#include "Renderer/IndexBuffer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/TextureView.hpp"

#include "Camera/CameraController.hpp"
#include "Core/InputDispatcher.hpp"

#include "GUI/Form.hpp"
#include "GUI/Label.hpp"

#include "Core/StringUtils.hpp"
#include "Vesper/Shapes/MeshLoader.hpp"

#include <future>
#include <algorithm>
#include <numeric>

#include "Models/Skybox.hpp"

#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/RenderPasses/ShadowPass.hpp"
#include "Renderer/RenderPasses/LiquidRenderPass.hpp"

#include "Geometry/TriangleMesh.hpp"
#include "Geometry/TriangleMeshBatch.hpp"
#include "Geometry/MeshGeometry.hpp"

#include "Lights/DirectionalLight.hpp"

#include "Models/BoxVisualizer.hpp"
#include "Models/FluidSimulation.hpp"
#include "Techniques/CascadedShadowMapper.hpp"

#include "GUI/FluidSimulationPanel.hpp"
#include "GUI/Slider.hpp"

namespace crisp
{
    namespace
    {
        std::ostream& operator<<(std::ostream& out, const glm::vec3& vec)
        {
            out << "[" << vec.x << ", " << vec.y << ", " << vec.z << "]\n";
            return out;
        }
    }

    Scene::Scene(VulkanRenderer* renderer, InputDispatcher* inputDispatcher, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_device(m_renderer->getDevice())
    {
        m_cameraController = std::make_unique<CameraController>(inputDispatcher);
        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        inputDispatcher->keyPressed.subscribe([this](int key, int mode)
        {
            if (key == GLFW_KEY_R)
            {
                m_renderMode = m_renderMode == 1 ? 0 : 1;
            }

            if (key == GLFW_KEY_V)
            {
                m_boxVisualizer->updateFrusta(m_shadowMapper.get(), m_cameraController.get());
            }
        });

        

        m_transforms.resize(27);
        for (uint32_t i = 0; i < 25; i++)
        {
            float xTrans = (i / 5) * 5.0f;
            float zTrans = (i % 5) * 5.0f;
            m_transforms[i].M = glm::translate(glm::vec3(xTrans - 10.0f, 0.5f, zTrans - 10.0f));
        }
        m_transforms[25].M = glm::translate(glm::vec3(0.0f, -1.0f, 0.0f)) * glm::scale(glm::vec3(50.0f, 1.0f, 50.0f));
        m_transforms[26].M = glm::translate(glm::vec3(2.5f, -1.0f, 0.0f)) * glm::rotate(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(3.0f));
        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(Transforms), BufferUpdatePolicy::PerFrame);

        uint32_t numCascades = 4;
        DirectionalLight light(glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f)));
        m_shadowMapper = std::make_unique<CascadedShadowMapper>(m_renderer, light, numCascades, m_transformsBuffer.get());

        m_planeGeometry  = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("plane.obj",  { VertexAttribute::Position, VertexAttribute::Normal }));
        m_sphereGeometry = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("sphere.obj", { VertexAttribute::Position, VertexAttribute::Normal }));

        m_linearClampSampler     = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        m_nearestNeighborSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_scenePass = std::make_unique<SceneRenderPass>(m_renderer);
        m_skybox = std::make_unique<Skybox>(m_renderer, m_scenePass.get());

        m_blinnPhongPipeline = std::make_unique<BlinnPhongPipeline>(m_renderer, m_scenePass.get());
        auto firstSet = m_blinnPhongPipeline->allocateDescriptorSet(0);
        for (auto& descGroup : m_blinnPhongDescGroups)
            descGroup = { firstSet, m_blinnPhongPipeline->allocateDescriptorSet(1) };

        m_blinnPhongDescGroups[0].postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(Transforms)));
        m_blinnPhongDescGroups[0].postBufferUpdate(0, 1, m_shadowMapper->getLightTransformsBuffer()->getDescriptorInfo());
        m_blinnPhongDescGroups[0].postBufferUpdate(0, 2, m_cameraBuffer->getDescriptorInfo());
        for (uint32_t frameIdx = 0; frameIdx < VulkanRenderer::NumVirtualFrames; frameIdx++)
        {
            for (uint32_t cascadeIdx = 0; cascadeIdx < numCascades; cascadeIdx++)
            {
                m_blinnPhongDescGroups[frameIdx].postImageUpdate(1, 0, cascadeIdx, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_nearestNeighborSampler->getHandle(), m_shadowMapper->getRenderPass()->getAttachmentView(cascadeIdx, frameIdx), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            }
            m_blinnPhongDescGroups[frameIdx].flushUpdates(m_device);
        }

        m_bunnyGeometry  = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("Nanosuit.obj",  { VertexAttribute::Position, VertexAttribute::Normal }));
        m_boxVisualizer = std::make_unique<BoxVisualizer>(m_renderer, 4, 4, m_scenePass.get());

        m_liquidPass = std::make_unique<LiquidRenderPass>(m_renderer);

        m_liquidNormalPipeline = std::make_unique<NormalPipeline>(m_renderer, m_liquidPass.get());
        m_normalDesc = { m_liquidNormalPipeline->allocateDescriptorSet(0) };
        m_normalDesc.postBufferUpdate(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_transformsBuffer->getDescriptorInfo(0, sizeof(Transforms)));
        m_normalDesc.flushUpdates(m_device);
        
        m_liquidPipeline = std::make_unique<LiquidPipeline>(m_renderer, m_liquidPass.get());
        
        for (uint32_t i = 0; i < VulkanRenderer::NumVirtualFrames; ++i)
        {
            m_liquidDesc[i] = { m_liquidPipeline->allocateDescriptorSet(0) };
            m_liquidDesc[i].postImageUpdate(0,  0, { nullptr, m_liquidPass->getAttachmentView(0, i), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            m_liquidDesc[i].postImageUpdate(0,  1, { m_linearClampSampler->getHandle(), m_scenePass->getAttachmentView(SceneRenderPass::Opaque, i), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            m_liquidDesc[i].postImageUpdate(0,  3, { m_nearestNeighborSampler->getHandle(), m_scenePass->getAttachmentView(SceneRenderPass::Depth, i), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            m_liquidDesc[i].postImageUpdate(0,  4, m_skybox->getSkyboxView()->getDescriptorInfo(m_linearClampSampler->getHandle()));
            m_liquidDesc[i].postBufferUpdate(0, 2, m_cameraBuffer->getDescriptorInfo());
            m_liquidDesc[i].flushUpdates(m_device);
        }

        m_fluidSimulation = std::make_unique<FluidSimulation>(m_renderer, m_scenePass.get());
        inputDispatcher->keyPressed.subscribe<FluidSimulation, &FluidSimulation::onKeyPressed>(m_fluidSimulation.get());

        auto fluidPanel = std::make_shared<gui::FluidSimulationPanel>(app->getForm(), m_fluidSimulation.get());
        app->getForm()->add(fluidPanel);

        initRenderTargetResources();
    }

    Scene::~Scene()
    {
    }

    void Scene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_scenePass->recreate();
        m_liquidPass->recreate();

        //m_sceneImageView = m_liquidPass->createRenderTargetView(LiquidRenderPass::LiquidMask);
        m_sceneImageView = m_scenePass->createRenderTargetView(SceneRenderPass::Opaque);
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_sceneImageView->getDescriptorInfo());
        m_sceneDescSetGroup.flushUpdates(m_device);
    }

    void Scene::update(float dt)
    {
        auto viewChanged = m_cameraController->update(dt);
        if (viewChanged)
        {
            auto& V = m_cameraController->getViewMatrix();
            auto& P = m_cameraController->getProjectionMatrix();

            //m_shadowMapper->recalculateLightProjections(m_cameraController->getCamera(), 50.0f, ParallelSplit::Logarithmic);
            //
            //
            //for (auto& trans : m_transforms)
            //{
            //    trans.MV  = V * trans.M;
            //    trans.MVP = P * trans.MV;
            //}
            //
            m_transformsBuffer->updateStagingBuffer(m_transforms.data(), m_transforms.size() * sizeof(Transforms));
            m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));
            //m_boxVisualizer->update(V, P);

            m_skybox->updateTransforms(P, V);
        }
        m_fluidSimulation->update(m_cameraController->getViewMatrix(), m_cameraController->getProjectionMatrix(), dt);
    }

    void Scene::render()
    {
        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();
            
            m_skybox->updateDeviceBuffers(commandBuffer, frameIdx);
            
            m_transformsBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            m_cameraBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            //m_shadowMapper->update(commandBuffer, frameIdx);
            //m_boxVisualizer->updateDeviceBuffers(commandBuffer, frameIdx);
            m_fluidSimulation->updateDeviceBuffers(commandBuffer, frameIdx);
        });

        m_renderer->enqueueDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();
            
            //m_shadowMapper->draw(commandBuffer, frameIdx, [this](VkCommandBuffer commandBuffer, uint32_t frameIdx, DescriptorSetGroup& descSets, VulkanPipeline* pipeline, uint32_t cascadeIdx)
            //{
            //    uint32_t cascade = cascadeIdx;
            //    pipeline->bind(commandBuffer);
            //    vkCmdPushConstants(commandBuffer, pipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &cascade);
            //
            //    for (uint32_t i = 0; i < 25; i++)
            //    {
            //        descSets.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + i * sizeof(Transforms));
            //        descSets.bind(commandBuffer, pipeline->getPipelineLayout());
            //        m_sphereGeometry->bindGeometryBuffers(commandBuffer);
            //        m_sphereGeometry->draw(commandBuffer);
            //    }
            //
            //    descSets.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 26 * sizeof(Transforms));
            //    descSets.bind(commandBuffer, pipeline->getPipelineLayout());
            //    m_bunnyGeometry->bindGeometryBuffers(commandBuffer);
            //    m_bunnyGeometry->draw(commandBuffer);
            //});
        });

        m_renderer->enqueueDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            m_scenePass->begin(commandBuffer);
            //m_blinnPhongPipeline->bind(commandBuffer);
            //m_blinnPhongDescGroups[frameIdx].setDynamicOffset(1, m_shadowMapper->getLightTransformsBuffer()->getDynamicOffset(frameIdx));
            //m_blinnPhongDescGroups[frameIdx].setDynamicOffset(2, m_cameraBuffer->getDynamicOffset(frameIdx));

            //for (uint32_t i = 0; i < 25; i++)
            //{
            //    m_blinnPhongDescGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + i * sizeof(Transforms));
            //    m_blinnPhongDescGroups[frameIdx].bind(commandBuffer, m_blinnPhongPipeline->getPipelineLayout());
            //    vkCmdPushConstants(commandBuffer, m_blinnPhongPipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(unsigned int), &m_renderMode);
            //    m_sphereGeometry->bindGeometryBuffers(commandBuffer);
            //    m_sphereGeometry->draw(commandBuffer);
            //}

            //m_blinnPhongDescGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 25 * sizeof(Transforms));
            //m_blinnPhongDescGroups[frameIdx].bind(commandBuffer, m_blinnPhongPipeline->getPipelineLayout());
            //m_planeGeometry->bindGeometryBuffers(commandBuffer);
            //m_planeGeometry->draw(commandBuffer);

            //m_boxVisualizer->render(commandBuffer, frameIdx);
            m_fluidSimulation->draw(commandBuffer, frameIdx);
            //m_skybox->draw(commandBuffer, frameIdx);

            m_scenePass->end(commandBuffer);

            //m_liquidPass->begin(commandBuffer);
            //m_liquidNormalPipeline->bind(commandBuffer);
            //m_normalDesc.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 26 * sizeof(Transforms));
            //m_normalDesc.bind(commandBuffer, m_liquidNormalPipeline->getPipelineLayout());
            //
            //m_bunnyGeometry->bindGeometryBuffers(commandBuffer);
            //m_bunnyGeometry->draw(commandBuffer);
            //
            //m_liquidPass->nextSubpass(commandBuffer);
            //
            //m_liquidPipeline->bind(commandBuffer);
            //m_liquidDesc[frameIdx].setDynamicOffset(0, m_cameraBuffer->getDynamicOffset(frameIdx));
            //m_liquidDesc[frameIdx].bind(commandBuffer, m_liquidPipeline->getPipelineLayout());
            //m_renderer->drawFullScreenQuad(commandBuffer);
            //
            //m_liquidPass->end(commandBuffer);
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

    void Scene::initRenderTargetResources()
    {
        m_fsQuadPipeline = std::make_unique<FullScreenQuadPipeline>(m_renderer, m_renderer->getDefaultRenderPass(), true);
        m_sceneDescSetGroup = { m_fsQuadPipeline->allocateDescriptorSet(FullScreenQuadPipeline::DisplayedImage) };

        //m_sceneImageView = m_liquidPass->createRenderTargetView(LiquidRenderPass::LiquidMask);
        m_sceneImageView = m_scenePass->createRenderTargetView(SceneRenderPass::Opaque);
        m_sceneDescSetGroup.postImageUpdate(0, 0, VK_DESCRIPTOR_TYPE_SAMPLER, m_sceneImageView->getDescriptorInfo(m_linearClampSampler->getHandle()));
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_sceneImageView->getDescriptorInfo());
        m_sceneDescSetGroup.flushUpdates(m_device);
    }
}