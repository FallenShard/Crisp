#include "AmbientOcclusionScene.hpp"

#include <random>

#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "Core/InputDispatcher.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "Camera/CameraController.hpp"

#include "Renderer/UniformBuffer.hpp"
#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/RenderPasses/AmbientOcclusionPass.hpp"
#include "Renderer/Pipelines/UniformColorPipeline.hpp"
#include "Renderer/Pipelines/NormalPipeline.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Renderer/Pipelines/SsaoPipeline.hpp"
#include "Renderer/TextureView.hpp"
#include "Renderer/Texture.hpp"

#include "vulkan/VulkanSampler.hpp"

#include "Geometry/MeshGeometry.hpp"
#include "Geometry/TriangleMesh.hpp"

#include "Models/Skybox.hpp"

#include <Vesper/Math/Warp.hpp>

#include "GUI/Form.hpp"
#include "AmbientOcclusionPanel.hpp"

namespace crisp
{
    AmbientOcclusionScene::AmbientOcclusionScene(VulkanRenderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
        , m_app(app)
        , m_numSamples(128)
        , m_radius(0.5f)
    {
        m_cameraController = std::make_unique<CameraController>(m_app->getWindow());
        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        std::default_random_engine randomEngine(std::random_device{}());
        std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
        glm::vec4 samples[512];
        for (int i = 0; i < 512; i++)
        {
            float x = distribution(randomEngine);
            float y = distribution(randomEngine);
            float r = distribution(randomEngine);
            glm::vec3 s = r * vesper::Warp::squareToUniformHemisphere(glm::vec2(x, y));
            float scale = float(i) / float(512.0f);
            scale = glm::mix(0.1f, 1.0f, scale * scale);
            samples[i].x = s.x * scale;
            samples[i].y = s.y * scale;
            samples[i].z = s.z * scale;
            samples[i].w = 1.0f;
        }

        std::vector<glm::vec4> noiseTex;
        for (int i = 0; i < 16; i++)
        {
            glm::vec3 s(distribution(randomEngine) * 2.0f - 1.0f, distribution(randomEngine) * 2.0f - 1.0f, 0.0f);
            s = glm::normalize(s);
            noiseTex.push_back(glm::vec4(s, 1.0f));
        }

        m_noiseTex = std::make_unique<Texture>(m_renderer, VkExtent3D{ 4, 4, 1 }, 1, VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_noiseTex->fill(noiseTex.data(), sizeof(glm::vec4) * noiseTex.size());
        m_noiseMapView = m_noiseTex->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1);
        m_noiseSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

        m_samplesBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(samples), BufferUpdatePolicy::Constant, samples);

        m_transforms.resize(2);
        m_transforms[0].M = glm::translate(glm::vec3(0.0f, -1.0f, 0.0f)) * glm::scale(glm::vec3(50.0f, 1.0f, 50.0f));
        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(Transforms), BufferUpdatePolicy::PerFrame);

        m_scenePass = std::make_unique<SceneRenderPass>(m_renderer);
        m_aoPass = std::make_unique<AmbientOcclusionPass>(m_renderer);

        m_uniformColorPipeline = std::make_unique<UniformColorPipeline>(m_renderer, m_scenePass.get());
        m_unifColorSetGroup = { m_uniformColorPipeline->allocateDescriptorSet(0) };
        m_unifColorSetGroup.postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(Transforms)));
        m_unifColorSetGroup.flushUpdates(m_device);

        m_nearestSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_normalPipeline = std::make_unique<NormalPipeline>(m_renderer, m_scenePass.get());

        m_ssaoPipeline = std::make_unique<SsaoPipeline>(m_renderer, m_aoPass.get());
        for (int i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
        {
            m_ssaoSetGroups[i] = { m_ssaoPipeline->allocateDescriptorSet(0) };
            m_ssaoSetGroups[i].postImageUpdate(0,  0, { m_nearestSampler->getHandle(), m_scenePass->getAttachmentView(0, i), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            m_ssaoSetGroups[i].postBufferUpdate(0, 1, m_cameraBuffer->getDescriptorInfo());
            m_ssaoSetGroups[i].postBufferUpdate(0, 2, m_samplesBuffer->getDescriptorInfo());
            m_ssaoSetGroups[i].postImageUpdate(0, 3, m_noiseMapView->getDescriptorInfo(m_noiseSampler->getHandle()));
            m_ssaoSetGroups[i].flushUpdates(m_device);
        }

        m_linearClampSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        m_fsQuadPipeline = std::make_unique<FullScreenQuadPipeline>(m_renderer, m_renderer->getDefaultRenderPass(), true);
        m_sceneDescSetGroup = { m_fsQuadPipeline->allocateDescriptorSet(FullScreenQuadPipeline::DisplayedImage) };

        m_sceneImageView = m_aoPass->createRenderTargetView(0);
        m_sceneDescSetGroup.postImageUpdate(0, 0, VK_DESCRIPTOR_TYPE_SAMPLER, m_sceneImageView->getDescriptorInfo(m_linearClampSampler->getHandle()));
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_sceneImageView->getDescriptorInfo());
        m_sceneDescSetGroup.flushUpdates(m_device);

        m_planeGeometry = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("plane.obj", { VertexAttribute::Position }));

        m_buddhaGeometry = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("sponza_fixed.obj", { VertexAttribute::Position, VertexAttribute::Normal }));

        m_skybox = std::make_unique<Skybox>(m_renderer, m_scenePass.get());

        auto panel = std::make_unique<gui::AmbientOcclusionPanel>(app->getForm(), this);
        m_app->getForm()->add(std::move(panel));
    }

    AmbientOcclusionScene::~AmbientOcclusionScene()
    {
        m_app->getForm()->remove("ambientOcclusionPanel");
    }

    void AmbientOcclusionScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_scenePass->recreate();
        m_aoPass->recreate();
        m_sceneImageView = m_aoPass->createRenderTargetView(0);
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_sceneImageView->getDescriptorInfo());
        m_sceneDescSetGroup.flushUpdates(m_device);
    }

    void AmbientOcclusionScene::update(float dt)
    {
        auto viewChanged = m_cameraController->update(dt);
        if (viewChanged)
        {
            auto& V = m_cameraController->getViewMatrix();
            auto& P = m_cameraController->getProjectionMatrix();

            for (auto& trans : m_transforms)
            {
                trans.MV  = V * trans.M;
                trans.MVP = P * trans.MV;
            }
            m_transformsBuffer->updateStagingBuffer(m_transforms.data(), m_transforms.size() * sizeof(Transforms));
            m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

            m_skybox->updateTransforms(P, V);
        }
    }

    void AmbientOcclusionScene::render()
    {
        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            m_skybox->updateDeviceBuffers(commandBuffer, frameIdx);

            m_transformsBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            m_cameraBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
        });

        m_renderer->enqueueDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            m_scenePass->begin(commandBuffer);
            m_uniformColorPipeline->bind(commandBuffer);

            glm::vec4 color = glm::vec4(0.2f, 0.0f, 0.0f, 1.0f);
            m_uniformColorPipeline->setPushConstant(commandBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, 0, color);

            m_unifColorSetGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx));
            m_unifColorSetGroup.bind(commandBuffer, m_uniformColorPipeline->getPipelineLayout());

            m_planeGeometry->bindGeometryBuffers(commandBuffer);
            m_planeGeometry->draw(commandBuffer);


            m_normalPipeline->bind(commandBuffer);
            m_unifColorSetGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 1 * sizeof(Transforms));
            m_unifColorSetGroup.bind(commandBuffer, m_normalPipeline->getPipelineLayout());
            m_buddhaGeometry->bindGeometryBuffers(commandBuffer);
            m_buddhaGeometry->draw(commandBuffer);

            m_skybox->draw(commandBuffer, frameIdx);

            m_scenePass->end(commandBuffer);

            VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            imageMemoryBarrier.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
            imageMemoryBarrier.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageMemoryBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageMemoryBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.image                           = m_scenePass->getColorAttachment();
            imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
            imageMemoryBarrier.subresourceRange.levelCount     = 1;
            imageMemoryBarrier.subresourceRange.baseArrayLayer = frameIdx;
            imageMemoryBarrier.subresourceRange.layerCount     = 1;
            imageMemoryBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

            m_renderer->finish();

            m_aoPass->begin(commandBuffer);

            m_ssaoPipeline->bind(commandBuffer);
            m_ssaoSetGroups[frameIdx].setDynamicOffset(0, m_cameraBuffer->getDynamicOffset(frameIdx));
            m_ssaoSetGroups[frameIdx].bind(commandBuffer, m_ssaoPipeline->getPipelineLayout());
            m_ssaoPipeline->setPushConstant(commandBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, 0, m_numSamples);
            m_ssaoPipeline->setPushConstant(commandBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(m_numSamples), m_radius);
            
            m_renderer->drawFullScreenQuad(commandBuffer);
            
            m_aoPass->end(commandBuffer);

            VkImageMemoryBarrier anotherMemBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            anotherMemBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            anotherMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            anotherMemBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            anotherMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            anotherMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            anotherMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            anotherMemBarrier.image = m_aoPass->getColorAttachment();
            anotherMemBarrier.subresourceRange.baseMipLevel = 0;
            anotherMemBarrier.subresourceRange.levelCount = 1;
            anotherMemBarrier.subresourceRange.baseArrayLayer = frameIdx;
            anotherMemBarrier.subresourceRange.layerCount = 1;
            anotherMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &anotherMemBarrier);
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

    void AmbientOcclusionScene::setNumSamples(int numSamples)
    {
        m_numSamples = numSamples;
    }

    void AmbientOcclusionScene::setRadius(double radius)
    {
        m_radius = static_cast<float>(radius);
    }
}