#include "Scene.hpp"

#include "Application.hpp"

#include "vulkan/VulkanRenderer.hpp"
#include "vulkan/Pipelines/UniformColorPipeline.hpp"
#include "vulkan/Pipelines/PointSphereSpritePipeline.hpp"
#include "vulkan/Pipelines/FullScreenQuadPipeline.hpp"
#include "vulkan/Pipelines/LiquidPipeline.hpp"
#include "vulkan/IndexBuffer.hpp"
#include "vulkan/UniformBuffer.hpp"

#include "Camera/CameraController.hpp"
#include "Core/InputDispatcher.hpp"

#include "GUI/Form.hpp"
#include "GUI/Label.hpp"

#include "Core/StringUtils.hpp"
#include "Vesper/Shapes/MeshLoader.hpp"

#include "Models/Skybox.hpp"

#include "vulkan/RenderPasses/SceneRenderPass.hpp"

namespace crisp
{
    namespace
    {
        glm::mat4 invertYaxis = glm::scale(glm::vec3(1.0f, -1.0f, 1.0f));

        unsigned int particles = 0;

        unsigned int numFaces = 0;

        struct Vertex
        {
            glm::vec3 position;
            glm::vec3 normal;

            Vertex() {}
            Vertex(const glm::vec3& pos, const glm::vec3& n) : position(pos), normal(n) {}

            static std::vector<Vertex> interleave(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals)
            {
                std::vector<Vertex> result;
                result.reserve(positions.size());

                for (size_t i = 0; i < positions.size(); ++i)
                {
                    result.emplace_back(positions[i], normals[i]);
                }

                return result;
            }
        };
    }

    Scene::Scene(VulkanRenderer* renderer, InputDispatcher* inputDispatcher, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_device(m_renderer->getDevice())
    {
        m_scenePass = std::make_unique<SceneRenderPass>(m_renderer);
        m_renderer->registerRenderPass(SceneRenderPassId, m_scenePass.get());

        m_linearClampSampler = m_device->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        initRenderTargetResources();

        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        std::vector<glm::uvec3> faces;

        vesper::MeshLoader meshLoader;
        meshLoader.load("sphere.obj", positions, normals, texCoords, faces);

        auto vertices = Vertex::interleave(positions, normals);

        //float cubeSize = 1.0f;
        //float increment = 0.05f;
        //for (float x = -cubeSize; x <= cubeSize; x += increment)
        //    for (float y = -cubeSize / 2; y <= cubeSize / 2; y += increment)
        //        for (float z = -cubeSize; z <= cubeSize; z += increment)
        //            positions.emplace_back(x, y, z);

        m_skybox = std::make_unique<Skybox>(m_renderer, m_scenePass.get());

        m_indexBuffer = std::make_unique<IndexBuffer>(m_device, faces);
        numFaces = static_cast<unsigned int>(faces.size());
        m_buffer      = std::make_unique<VertexBuffer>(m_device, vertices);
        m_vertexBufferGroup =
        {
            { m_buffer->get(), 0 }
        };

        m_pipeline = std::make_unique<LiquidPipeline>(m_renderer, m_scenePass.get());
        //m_psPipeline = std::make_unique<PointSphereSpritePipeline>(m_renderer, m_scenePass.get());
        //m_descriptorSetGroup =
        //{
        //    m_psPipeline->allocateDescriptorSet(0),
        //    m_psPipeline->allocateDescriptorSet(1)
        //};
        auto set = m_pipeline->allocateDescriptorSet(0);
        auto sets = m_pipeline->allocateDescriptorSet(1, VulkanRenderer::NumVirtualFrames);
        for (size_t i = 0; i < sets.size(); ++i)
            m_descriptorSetGroups.push_back({ set, sets[i] });

        // Transform uniform buffer
        auto extent = m_renderer->getSwapChainExtent();
        auto aspect = static_cast<float>(extent.width) / extent.height;
        auto proj   = glm::perspective(glm::radians(35.0f), aspect, 1.0f, 100.0f);

        m_transforms.MV  = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
        m_transforms.MVP = invertYaxis * proj * m_transforms.MV;

        m_transformsBuffer = std::make_unique<UniformBuffer>(m_device, sizeof(Transforms), BufferUpdatePolicy::PerFrame);

        m_cameraBuffer = std::make_unique<UniformBuffer>(m_device, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        // Color descriptor
        //m_params.radius           = 0.025f;
        //m_params.screenSpaceScale = proj[1][1] * extent.height;

        //m_particleParamsBuffer = std::make_unique<UniformBuffer>(m_device, sizeof(ParticleParams), BufferUpdatePolicy::PerFrame);
        //m_particleParamsBuffer->updateStagingBuffer(&m_params, sizeof(ParticleParams));

        // Update descriptor sets
        m_descriptorSetGroups[0].postBufferUpdate(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, { m_transformsBuffer->get(), 0, sizeof(Transforms) });
        m_descriptorSetGroups[0].postBufferUpdate(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, { m_cameraBuffer->get(), 0, sizeof(CameraParameters) });
        m_descriptorSetGroups[0].postImageUpdate(0, 2, VK_DESCRIPTOR_TYPE_SAMPLER, { m_linearClampSampler, 0, VK_IMAGE_LAYOUT_UNDEFINED });
        m_descriptorSetGroups[0].postImageUpdate(0, 3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, { m_linearClampSampler, m_skybox->getSkyboxView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });

        for (uint32_t i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
        {
            m_descriptorSetGroups[i].postImageUpdate(1, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, { m_linearClampSampler, m_scenePass->getAttachmentView(SceneRenderPass::Opaque, i), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            m_descriptorSetGroups[i].flushUpdates(m_device);
        }

        m_cameraController = std::make_unique<CameraController>(inputDispatcher);
    }

    Scene::~Scene()
    {
        vkDestroyImageView(m_device->getHandle(), m_sceneImageView, nullptr);
        vkDestroySampler(m_device->getHandle(), m_linearClampSampler, nullptr);
    }

    void Scene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_scenePass->recreate();

        m_pipeline->resize(width, height);
        m_skybox->resize(width, height);

        m_fsQuadPipeline->resize(width, height);

        for (uint32_t i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
        {
            m_descriptorSetGroups[i].postImageUpdate(1, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, { m_linearClampSampler, m_scenePass->getAttachmentView(SceneRenderPass::Opaque, i), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            m_descriptorSetGroups[i].flushUpdates(m_device);
        }

        //m_psPipeline->resize(width, height);
        //m_params.screenSpaceScale = m_cameraController->getCamera().getProjectionMatrix()[1][1] * m_renderer->getSwapChainExtent().height;
        //m_particleParamsBuffer->updateStagingBuffer(&m_params, sizeof(ParticleParams));

        vkDestroyImageView(m_device->getHandle(), m_sceneImageView, nullptr);
        m_sceneImageView = m_device->createImageView(m_scenePass->getColorAttachment(SceneRenderPass::Composited), VK_IMAGE_VIEW_TYPE_2D_ARRAY, m_scenePass->getColorFormat(), VK_IMAGE_ASPECT_COLOR_BIT, 0, VulkanRenderer::NumVirtualFrames);

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = m_sceneImageView;
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo);
        m_sceneDescSetGroup.flushUpdates(m_device);
    }

    void Scene::update(float dt)
    {
        auto viewChanged = m_cameraController->update(dt);
        if (viewChanged)
        {
            auto& V = m_cameraController->getCamera().getViewMatrix();
            auto& P = m_cameraController->getCamera().getProjectionMatrix();

            m_transforms.MV = V * m_transforms.M;
            m_transforms.MVP = invertYaxis * P * m_transforms.MV;

            m_transformsBuffer->updateStagingBuffer(&m_transforms, sizeof(Transforms));
            m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

            m_skybox->updateTransforms(invertYaxis * P, V);
        }
    }

    void Scene::render()
    {
        m_renderer->addCopyAction([this](VkCommandBuffer& commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            m_transformsBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            m_cameraBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            //m_particleParamsBuffer->updateDeviceBuffer(commandBuffer, m_renderer->getCurrentVirtualFrameIndex());

            m_skybox->updateDeviceBuffers(commandBuffer, frameIdx);
        });

        m_renderer->addDrawAction([this](VkCommandBuffer& commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();
            
            m_skybox->draw(commandBuffer, frameIdx, 0);
            //m_descriptorSetGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx));
            //m_descriptorSetGroup.setDynamicOffset(1, m_particleParamsBuffer->getDynamicOffset(frameIdx));
            //m_descriptorSetGroup.bind(commandBuffer, m_psPipeline->getPipelineLayout());
            //m_vertexBufferGroup.bind(commandBuffer);
            //vkCmdDraw(commandBuffer, particles, 1, 0, 0);

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

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getHandle());
            m_descriptorSetGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx));
            m_descriptorSetGroups[frameIdx].setDynamicOffset(1, m_cameraBuffer->getDynamicOffset(frameIdx));
            m_descriptorSetGroups[frameIdx].bind(commandBuffer, m_pipeline->getPipelineLayout());
            
            m_vertexBufferGroup.bind(commandBuffer);
            m_indexBuffer->bind(commandBuffer, 0);
            vkCmdDrawIndexed(commandBuffer, numFaces * 3, 1, 0, 0, 0);

            m_skybox->draw(commandBuffer, frameIdx, 1);

        }, SceneRenderPassId);

        m_renderer->addDrawAction([this](VkCommandBuffer& commandBuffer)
        {
            VkViewport viewport;
            viewport.x = viewport.y = 0;
            viewport.width    = static_cast<float>(m_renderer->getSwapChainExtent().width);
            viewport.height   = static_cast<float>(m_renderer->getSwapChainExtent().height);
            viewport.minDepth = 0;
            viewport.maxDepth = 1;
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fsQuadPipeline->getHandle());
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            m_sceneDescSetGroup.bind(commandBuffer, m_fsQuadPipeline->getPipelineLayout());

            unsigned int pushConst = m_renderer->getCurrentVirtualFrameIndex();
            vkCmdPushConstants(commandBuffer, m_fsQuadPipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(unsigned int), &pushConst);

            m_renderer->drawFullScreenQuad(commandBuffer);
        }, VulkanRenderer::DefaultRenderPassId);
    }

    void Scene::initRenderTargetResources()
    {
        m_fsQuadPipeline = std::make_unique<FullScreenQuadPipeline>(m_renderer, m_renderer->getDefaultRenderPass());
        // Create descriptor set for the image
        m_sceneDescSetGroup =
        {
            m_fsQuadPipeline->allocateDescriptorSet(FullScreenQuadPipeline::DisplayedImage)
        };

        // Create a view to the render target
        m_sceneImageView = m_device->createImageView(m_scenePass->getColorAttachment(SceneRenderPass::Composited), VK_IMAGE_VIEW_TYPE_2D_ARRAY, m_scenePass->getColorFormat(), VK_IMAGE_ASPECT_COLOR_BIT, 0, VulkanRenderer::NumVirtualFrames);

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = m_sceneImageView;
        imageInfo.sampler     = m_linearClampSampler;
        m_sceneDescSetGroup.postImageUpdate(0, 0, VK_DESCRIPTOR_TYPE_SAMPLER, imageInfo);
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo);
        m_sceneDescSetGroup.flushUpdates(m_device);
    }
}