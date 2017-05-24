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

        std::vector<glm::vec3> getFrustumPoints(float fovY, float aspectRatio, float zNear, float zFar, const glm::mat4& cameraToWorld)
        {
            auto tanA = std::tan(fovY / 2.0f);

            auto halfNearH = zNear * tanA;
            auto halfNearW = halfNearH * aspectRatio;

            auto halfFarH = zFar * tanA;
            auto halfFarW = halfFarH * aspectRatio;

            std::vector<glm::vec3> frustumPoints =
            {
                glm::vec3(-halfNearW, -halfNearH, -zNear),
                glm::vec3(+halfNearW, -halfNearH, -zNear),
                glm::vec3(-halfNearW, +halfNearH, -zNear),
                glm::vec3(+halfNearW, +halfNearH, -zNear),
                glm::vec3(-halfFarW, -halfFarH, -zFar),
                glm::vec3(+halfFarW, -halfFarH, -zFar),
                glm::vec3(-halfFarW, +halfFarH, -zFar),
                glm::vec3(+halfFarW, +halfFarH, -zFar)
            };

            std::vector<glm::vec3> result;
            std::transform(frustumPoints.begin(), frustumPoints.end(), std::back_inserter(result), [cameraToWorld](const glm::vec3& pt) { return glm::vec3(cameraToWorld * glm::vec4(pt, 1.0f)); });
            return result;
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

        m_shadowPass = std::make_unique<ShadowPass>(m_renderer);
        m_shadowMapPipelines.resize(3);
        m_shadowMapPipelines[0] = std::make_unique<ShadowMapPipeline>(m_renderer, m_shadowPass.get(), 0);
        m_shadowMapPipelines[1] = std::make_unique<ShadowMapPipeline>(m_renderer, m_shadowPass.get(), 1);
        m_shadowMapPipelines[2] = std::make_unique<ShadowMapPipeline>(m_renderer, m_shadowPass.get(), 2);
        m_shadowMapDescGroup = 
        {
            m_shadowMapPipelines[0]->allocateDescriptorSet(0)
        };

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

        
        m_shadowParameters[0].lightView = glm::lookAt(glm::vec3(5.0f, 5.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        //shadowParameters.lightProjection = invertYaxis * glm::perspective(glm::radians(70.0f), 1.0f, 0.1f, 50.0f);
        m_shadowParameters[1].lightProjection = invertYaxis * glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 1.0f, 20.0f);
        m_shadowParameters[2].lightViewProjection = m_shadowParameters[0].lightProjection * m_shadowParameters[1].lightView;

        m_shadowTransformsBuffer = std::make_unique<UniformBuffer>(m_renderer, 3 * sizeof(ShadowParameters), BufferUpdatePolicy::PerFrame, m_shadowParameters);

        m_blinnPhongDescGroups[0].postBufferUpdate(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, { m_transformsBuffer->get(), 0, sizeof(Transforms) });
        m_blinnPhongDescGroups[0].postBufferUpdate(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, { m_shadowTransformsBuffer->get(), 0, 3 * sizeof(ShadowParameters) });
        m_blinnPhongDescGroups[0].postBufferUpdate(0, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, { m_cameraBuffer->get(),     0, sizeof(CameraParameters) });
        m_blinnPhongDescGroups[0].postImageUpdate(1, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_linearClampSampler, m_shadowPass->getAttachmentView(0, 0), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        m_blinnPhongDescGroups[0].postImageUpdate(1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_linearClampSampler, m_shadowPass->getAttachmentView(1, 0), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        m_blinnPhongDescGroups[0].postImageUpdate(1, 0, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_linearClampSampler, m_shadowPass->getAttachmentView(2, 0), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        m_blinnPhongDescGroups[0].flushUpdates(m_device);

        m_blinnPhongDescGroups[1].postImageUpdate(1, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_linearClampSampler, m_shadowPass->getAttachmentView(0, 1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        m_blinnPhongDescGroups[1].postImageUpdate(1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_linearClampSampler, m_shadowPass->getAttachmentView(1, 1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        m_blinnPhongDescGroups[1].postImageUpdate(1, 0, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_linearClampSampler, m_shadowPass->getAttachmentView(2, 1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        m_blinnPhongDescGroups[1].flushUpdates(m_device);

        m_blinnPhongDescGroups[2].postImageUpdate(1, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_linearClampSampler, m_shadowPass->getAttachmentView(0, 2), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        m_blinnPhongDescGroups[2].postImageUpdate(1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_linearClampSampler, m_shadowPass->getAttachmentView(1, 2), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        m_blinnPhongDescGroups[2].postImageUpdate(1, 0, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_linearClampSampler, m_shadowPass->getAttachmentView(2, 2), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        m_blinnPhongDescGroups[2].flushUpdates(m_device);

        m_shadowMapDescGroup.postBufferUpdate(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, { m_transformsBuffer->get(), 0, sizeof(Transforms) });
        m_shadowMapDescGroup.postBufferUpdate(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, { m_shadowTransformsBuffer->get(), 0, sizeof(ShadowParameters) });
        m_shadowMapDescGroup.flushUpdates(m_device);
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

            auto zNear = m_cameraController->getCamera().getNearPlaneDistance();
            auto zFar = m_cameraController->getCamera().getFarPlaneDistance();

            calculateFrustumOBB(m_shadowParameters[0], zNear, 15.0f);
            calculateFrustumOBB(m_shadowParameters[1], 15.0f, 45.0f);
            calculateFrustumOBB(m_shadowParameters[2], 45.0f, zFar);
            m_shadowTransformsBuffer->updateStagingBuffer(m_shadowParameters, 3 * sizeof(ShadowParameters));

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
            m_shadowTransformsBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            m_skybox->updateDeviceBuffers(commandBuffer, frameIdx);
        });

        m_renderer->addDrawAction([this](VkCommandBuffer& commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            m_shadowPass->begin(commandBuffer);

            auto drawStuff = [this](VkCommandBuffer commandBuffer, uint32_t frameIdx, uint32_t cascadeIdx)
            {
                m_shadowMapPipelines[cascadeIdx]->bind(commandBuffer);
                m_shadowMapDescGroup.setDynamicOffset(1, m_shadowTransformsBuffer->getDynamicOffset(frameIdx) + cascadeIdx * sizeof(ShadowParameters));
                m_shadowMapDescGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 0 * sizeof(Transforms));
                m_shadowMapDescGroup.bind(commandBuffer, m_shadowMapPipelines[cascadeIdx]->getPipelineLayout());

                m_sphereGeometry->bindGeometryBuffers(commandBuffer);
                m_sphereGeometry->draw(commandBuffer);

                for (int i = 3; i < 33; ++i)
                {
                    m_shadowMapDescGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + i * sizeof(Transforms));
                    m_shadowMapDescGroup.bind(commandBuffer, m_shadowMapPipelines[cascadeIdx]->getPipelineLayout());

                    m_sphereGeometry->bindGeometryBuffers(commandBuffer);
                    m_sphereGeometry->draw(commandBuffer);
                }

                m_shadowMapDescGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 2 * sizeof(Transforms));
                m_shadowMapDescGroup.bind(commandBuffer, m_shadowMapPipelines[cascadeIdx]->getPipelineLayout());
                m_bunnyGeometry->bindGeometryBuffers(commandBuffer);
                m_bunnyGeometry->draw(commandBuffer);
            };

            drawStuff(commandBuffer, frameIdx, 0);

            vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
            drawStuff(commandBuffer, frameIdx, 1);

            vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
            drawStuff(commandBuffer, frameIdx, 2);

            m_shadowPass->end(commandBuffer);
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
            m_blinnPhongDescGroups[frameIdx].setDynamicOffset(1, m_shadowTransformsBuffer->getDynamicOffset(frameIdx));
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

    void Scene::calculateFrustumOBB(ShadowParameters& shadowParams, float zNear, float zFar)
    {
        // 1. obtain world space frustum points 
        auto fov = m_cameraController->getCamera().getFov();
        auto aspect = m_cameraController->getCamera().getAspectRatio();

        auto worldFrustumPoints = getFrustumPoints(fov, aspect, zNear, zFar, glm::inverse(m_cameraController->getCamera().getViewMatrix()));

        // 2. get center of frustum in world space
        auto frustumCenter = std::accumulate(worldFrustumPoints.begin(), worldFrustumPoints.end(), glm::vec3(0.0f)) * (1.0f / worldFrustumPoints.size());

        // 3. set up orthonormal basis for light space
        auto lightDir   = glm::normalize(glm::vec3(0.0f, -1.0f, -1.0f));
        auto lightUp    = glm::vec3(0.0f, 1.0f, 0.0f);
        auto lightRight = glm::normalize(glm::cross(lightDir, lightUp));
        lightUp = glm::normalize(glm::cross(lightRight, lightDir));

        auto lightPos = glm::vec3(0.0f, 0.0f, 0.0f);
        shadowParams.lightView = glm::lookAt(lightPos, lightPos + lightDir, glm::vec3(0.0f, 1.0f, 0.0f));

        // 4. Calculate light's OBB
        glm::vec3 minCorner(1000.0f);
        glm::vec3 maxCorner(-1000.0f);
        for (auto& pt : worldFrustumPoints)
        {
            auto lightViewPt = shadowParams.lightView * glm::vec4(pt, 1.0f);

            minCorner = glm::min(minCorner, glm::vec3(lightViewPt));
            maxCorner = glm::max(maxCorner, glm::vec3(lightViewPt));
        }

        glm::vec3 lengths(maxCorner - minCorner);
        auto cubeCenter = (minCorner + maxCorner) / 2.0f;
        auto squareSize = std::max(lengths.x, lengths.y);

        auto lightProj = glm::ortho(
            cubeCenter.x - (squareSize / 2.0f), cubeCenter.x + (squareSize / 2.0f),
            cubeCenter.y - (squareSize / 2.0f), cubeCenter.y + (squareSize / 2.0f),
            -maxCorner.z, -minCorner.z);

        shadowParams.lightProjection = invertYaxis * lightProj;

        shadowParams.lightViewProjection = shadowParams.lightProjection * shadowParams.lightView;
        //m_transforms[32].M = glm::translate(cubeCenter);

        //std::cout << "MIN: " << minCorner;
        //std::cout << "MAX: " << maxCorner;
        //std::cout << "FRUSTUM CENTER: " << frustumCenter;
        //std::cout << "FRUSTUM CENTER VIEW: " << m_shadowParameters.lightView * glm::vec4(frustumCenter, 1.0f);
        //std::cout << "FRUSTUM CENTER PROJ: " << m_shadowParameters.lightViewProjection * glm::vec4(frustumCenter, 1.0f);
        //std::cout << "=================================\n";
    }
}