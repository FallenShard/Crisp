#include "Scene.hpp"

#include "Application.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/Pipelines/UniformColorPipeline.hpp"
#include "Renderer/Pipelines/PointSphereSpritePipeline.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Renderer/Pipelines/LiquidPipeline.hpp"
#include "Renderer/Pipelines/BlinnPhongPipeline.hpp"
#include "Renderer/Pipelines/ShadowMapPipeline.hpp"
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

#include "Geometry/TriangleMesh.hpp"
#include "Geometry/TriangleMeshBatch.hpp"
#include "Geometry/MeshGeometry.hpp"

#include "Lights/DirectionalLight.hpp"

#include "Techniques/CascadedShadowMapper.hpp"

namespace crisp
{
    namespace
    {
        glm::mat4 invertYaxis = glm::scale(glm::vec3(1.0f, -1.0f, 1.0f));

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

        m_scenePass = std::make_unique<SceneRenderPass>(m_renderer);
        m_renderer->registerRenderPass(SceneRenderPassId, m_scenePass.get());

        m_linearClampSampler = m_device->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        initRenderTargetResources();

        m_skybox = std::make_unique<Skybox>(m_renderer, m_scenePass.get());

        m_blinnPhongPipeline = std::make_unique<BlinnPhongPipeline>(m_renderer, m_scenePass.get());

        auto firstSet = m_blinnPhongPipeline->allocateDescriptorSet(0);
        m_blinnPhongDescGroups[0] = 
        {
            firstSet,
            m_blinnPhongPipeline->allocateDescriptorSet(1)
        };
        m_blinnPhongDescGroups[1] =
        {
            firstSet,
            m_blinnPhongPipeline->allocateDescriptorSet(1)
        };

        m_blinnPhongDescGroups[2] =
        {
            firstSet,
            m_blinnPhongPipeline->allocateDescriptorSet(1)
        };

        m_sphereGeometry = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("sphere.obj", { VertexAttribute::Position, VertexAttribute::Normal }));
        m_planeGeometry  = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("plane.obj",  { VertexAttribute::Position, VertexAttribute::Normal }));
        m_bunnyGeometry  = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("bunny.obj",  { VertexAttribute::Position, VertexAttribute::Normal }));

        m_transforms.resize(33);
        m_transforms[0].M = glm::mat4(1.0f);
        m_transforms[1].M = glm::translate(glm::vec3(0.0f, -1.0f, 0.0f)) * glm::scale(glm::vec3(50.0f, 1.0f, 50.0f));
        m_transforms[2].M = glm::translate(glm::vec3(3.0f, -0.5f, 0.0f));

        for (int i = 3; i < 33; ++i)
        {
            m_transforms[i].M = glm::translate(glm::vec3(3.0f * float(i % 6 + 1), 0.0f, 3.0f * float(i / 6 + 1))) * glm::translate(glm::vec3(0.0f, 0.0f, -5.0f));
        }

        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(Transforms), BufferUpdatePolicy::PerFrame);

        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        uint32_t numCascades = 4;
        DirectionalLight light(glm::normalize(glm::vec3(0.0f, -1.0f, -1.0f)));
        m_shadowMapper = std::make_unique<CascadedShadowMapper>(m_renderer, light, numCascades, m_transformsBuffer.get());

        m_blinnPhongDescGroups[0].postBufferUpdate(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_transformsBuffer->getDescriptorInfo(0, sizeof(Transforms)));
        m_blinnPhongDescGroups[0].postBufferUpdate(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_shadowMapper->getLightTransformsBuffer()->getDescriptorInfo());
        m_blinnPhongDescGroups[0].postBufferUpdate(0, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_cameraBuffer->getDescriptorInfo());
        for (uint32_t frameIdx = 0; frameIdx < VulkanRenderer::NumVirtualFrames; frameIdx++)
        {
            for (uint32_t cascadeIdx = 0; cascadeIdx < numCascades; cascadeIdx++)
            {
                m_blinnPhongDescGroups[frameIdx].postImageUpdate(1, 0, cascadeIdx, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_linearClampSampler, m_shadowMapper->getRenderPass()->getAttachmentView(cascadeIdx, frameIdx), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            }
            m_blinnPhongDescGroups[frameIdx].flushUpdates(m_device);
        }
    }

    Scene::~Scene()
    {
        vkDestroySampler(m_device->getHandle(), m_linearClampSampler, nullptr);
    }

    void Scene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_scenePass->recreate();

        m_sceneImageView = m_scenePass->createRenderTargetView(SceneRenderPass::Composited);
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_sceneImageView->getDescriptorInfo());
        m_sceneDescSetGroup.flushUpdates(m_device);
    }

    void Scene::update(float dt)
    {
        auto viewChanged = m_cameraController->update(dt);
        if (viewChanged)
        {
            auto& V = m_cameraController->getCamera().getViewMatrix();
            auto invertedP = invertYaxis * m_cameraController->getCamera().getProjectionMatrix();

            m_shadowMapper->recalculateLightProjections(m_cameraController->getCamera(), 150.0f, ParallelSplit::Logarithmic);

            for (auto& trans : m_transforms)
            {
                trans.MV  = V * trans.M;
                trans.MVP = invertedP * trans.MV;
            }

            m_transformsBuffer->updateStagingBuffer(m_transforms.data(), m_transforms.size() * sizeof(Transforms));
            m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

            m_skybox->updateTransforms(invertedP, V);
        }
    }

    void Scene::render()
    {
        m_renderer->addCopyAction([this](VkCommandBuffer& commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();
            m_transformsBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            m_cameraBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            m_skybox->updateDeviceBuffers(commandBuffer, frameIdx);

            m_shadowMapper->update(commandBuffer, frameIdx);
        });

        m_renderer->addDrawAction([this](VkCommandBuffer& commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            m_shadowMapper->draw(commandBuffer, frameIdx, [this](VkCommandBuffer commandBuffer, uint32_t frameIdx, DescriptorSetGroup& descSets, VulkanPipeline* pipeline, uint32_t cascadeIdx)
            {
                uint32_t cascade = cascadeIdx;
                pipeline->bind(commandBuffer);
                
                descSets.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 0 * sizeof(Transforms));
                descSets.bind(commandBuffer, pipeline->getPipelineLayout());
                vkCmdPushConstants(commandBuffer, pipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &cascade);
                m_sphereGeometry->bindGeometryBuffers(commandBuffer);
                m_sphereGeometry->draw(commandBuffer);

                for (int i = 3; i < 33; ++i)
                {
                    descSets.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + i * sizeof(Transforms));
                    descSets.bind(commandBuffer, pipeline->getPipelineLayout());
                    vkCmdPushConstants(commandBuffer, pipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &cascade);
                    m_sphereGeometry->bindGeometryBuffers(commandBuffer);
                    m_sphereGeometry->draw(commandBuffer);
                }

                descSets.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 2 * sizeof(Transforms));
                descSets.bind(commandBuffer, pipeline->getPipelineLayout());
                vkCmdPushConstants(commandBuffer, pipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &cascade);
                m_bunnyGeometry->bindGeometryBuffers(commandBuffer);
                m_bunnyGeometry->draw(commandBuffer);
            });
        });

        m_renderer->addDrawAction([this](VkCommandBuffer& commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            VkImageMemoryBarrier transBarrier = {};
            transBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            transBarrier.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            transBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            transBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            transBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            transBarrier.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            transBarrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
            transBarrier.image                           = m_scenePass->getColorAttachment(SceneRenderPass::Opaque);
            transBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            transBarrier.subresourceRange.baseMipLevel   = 0;
            transBarrier.subresourceRange.levelCount     = 1;
            transBarrier.subresourceRange.baseArrayLayer = frameIdx;
            transBarrier.subresourceRange.layerCount     = 1;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &transBarrier);

            vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

            m_blinnPhongPipeline->bind(commandBuffer);
            m_blinnPhongDescGroups[frameIdx].setDynamicOffset(1, m_shadowMapper->getLightTransformsBuffer()->getDynamicOffset(frameIdx));
            m_blinnPhongDescGroups[frameIdx].setDynamicOffset(2, m_cameraBuffer->getDynamicOffset(frameIdx));

            m_blinnPhongDescGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 0 * sizeof(Transforms));
            m_blinnPhongDescGroups[frameIdx].bind(commandBuffer, m_blinnPhongPipeline->getPipelineLayout());
            m_sphereGeometry->bindGeometryBuffers(commandBuffer);
            m_sphereGeometry->draw(commandBuffer);

            for (int i = 3; i < 33; ++i)
            {
                m_blinnPhongDescGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + i * sizeof(Transforms));
                m_blinnPhongDescGroups[frameIdx].bind(commandBuffer, m_blinnPhongPipeline->getPipelineLayout());

                m_sphereGeometry->bindGeometryBuffers(commandBuffer);
                m_sphereGeometry->draw(commandBuffer);
            }

            m_blinnPhongDescGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 1 * sizeof(Transforms));
            m_blinnPhongDescGroups[frameIdx].bind(commandBuffer, m_blinnPhongPipeline->getPipelineLayout());
            m_planeGeometry->bindGeometryBuffers(commandBuffer);
            m_planeGeometry->draw(commandBuffer);

            m_blinnPhongDescGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 2 * sizeof(Transforms));
            m_blinnPhongDescGroups[frameIdx].bind(commandBuffer, m_blinnPhongPipeline->getPipelineLayout());
            m_bunnyGeometry->bindGeometryBuffers(commandBuffer);
            m_bunnyGeometry->draw(commandBuffer);

            m_skybox->draw(commandBuffer, frameIdx, 1);
        }, SceneRenderPassId);

        m_renderer->addDrawAction([this](VkCommandBuffer& commandBuffer)
        {
            m_fsQuadPipeline->bind(commandBuffer);
            m_renderer->setDefaultViewport(commandBuffer);
            m_sceneDescSetGroup.bind(commandBuffer, m_fsQuadPipeline->getPipelineLayout());
        
            unsigned int pushConst = m_renderer->getCurrentVirtualFrameIndex();
            vkCmdPushConstants(commandBuffer, m_fsQuadPipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(unsigned int), &pushConst);
            
            m_renderer->drawFullScreenQuad(commandBuffer);
        }, VulkanRenderer::DefaultRenderPassId);
    }

    void Scene::initRenderTargetResources()
    {
        m_fsQuadPipeline = std::make_unique<FullScreenQuadPipeline>(m_renderer, m_renderer->getDefaultRenderPass(), true);
        m_sceneDescSetGroup = { m_fsQuadPipeline->allocateDescriptorSet(FullScreenQuadPipeline::DisplayedImage) };

        m_sceneImageView = m_scenePass->createRenderTargetView(SceneRenderPass::Composited);
        m_sceneDescSetGroup.postImageUpdate(0, 0, VK_DESCRIPTOR_TYPE_SAMPLER, m_sceneImageView->getDescriptorInfo(m_linearClampSampler));
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_sceneImageView->getDescriptorInfo());
        m_sceneDescSetGroup.flushUpdates(m_device);
    }
}