#include "Scene.hpp"

#include "Application.hpp"

#include "vulkan/VulkanRenderer.hpp"
#include "vulkan/Pipelines/UniformColorPipeline.hpp"
#include "vulkan/Pipelines/PointSphereSpritePipeline.hpp"
#include "vulkan/Pipelines/FullScreenQuadPipeline.hpp"
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
    }

    Scene::Scene(VulkanRenderer* renderer, InputDispatcher* inputDispatcher, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_device(&m_renderer->getDevice())
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

        float cubeSize = 1.0f;
        float increment = 0.05f;
        for (float x = -cubeSize; x <= cubeSize; x += increment)
            for (float y = -cubeSize / 2; y <= cubeSize / 2; y += increment)
                for (float z = -cubeSize; z <= cubeSize; z += increment)
                    positions.emplace_back(x, y, z);

        particles = static_cast<unsigned int>(positions.size());

        std::cout << "Num particles: " << particles << std::endl;

        m_skybox = std::make_unique<Skybox>(m_renderer, m_scenePass.get());

        m_buffer      = std::make_unique<VertexBuffer>(m_device, positions);
        m_vertexBufferGroup =
        {
            { m_buffer->get(), 0 }
        };


        m_psPipeline = std::make_unique<PointSphereSpritePipeline>(m_renderer, m_scenePass.get());
        m_descriptorSetGroup =
        {
            m_psPipeline->allocateDescriptorSet(0),
            m_psPipeline->allocateDescriptorSet(1)
        };

        // Transform uniform buffer
        auto extent = m_renderer->getSwapChainExtent();
        auto aspect = static_cast<float>(extent.width) / extent.height;
        auto proj   = glm::perspective(glm::radians(35.0f), aspect, 1.0f, 100.0f);

        m_transforms.MV  = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
        m_transforms.MVP = invertYaxis * proj * m_transforms.MV;

        m_transformsBuffer = std::make_unique<UniformBuffer>(m_device, sizeof(Transforms), BufferUpdatePolicy::PerFrame);

        // Color descriptor
        m_params.radius           = 0.025f;
        m_params.screenSpaceScale = proj[1][1] * extent.height;

        m_particleParamsBuffer = std::make_unique<UniformBuffer>(m_device, sizeof(ParticleParams), BufferUpdatePolicy::PerFrame);
        m_particleParamsBuffer->updateStagingBuffer(&m_params, sizeof(ParticleParams));

        // Update descriptor sets
        m_descriptorSetGroup.postBufferUpdate(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, { m_transformsBuffer->get(), 0, sizeof(Transforms) });
        m_descriptorSetGroup.postBufferUpdate(1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, { m_particleParamsBuffer->get(), 0, sizeof(ParticleParams) });
        m_descriptorSetGroup.flushUpdates(m_device);

        m_cameraController = std::make_unique<CameraController>(inputDispatcher);
    }

    Scene::~Scene()
    {

    }

    void Scene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);
        m_psPipeline->resize(width, height);

        m_skybox->resize(width, height);

        m_params.screenSpaceScale = m_cameraController->getCamera().getProjectionMatrix()[1][1] * m_renderer->getSwapChainExtent().height;
        m_particleParamsBuffer->updateStagingBuffer(&m_params, sizeof(ParticleParams));

        m_scenePass->recreate();
        m_fsQuadPipeline->resize(width, height);
        vkDestroyImageView(m_device->getHandle(), m_sceneImageView, nullptr);
        m_sceneImageView = m_device->createImageView(m_scenePass->getColorAttachment(0), VK_IMAGE_VIEW_TYPE_2D_ARRAY, m_scenePass->getColorFormat(), VK_IMAGE_ASPECT_COLOR_BIT, 0, VulkanRenderer::NumVirtualFrames);

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
            m_transformsBuffer->updateStagingBuffer(&m_transforms, sizeof(Transforms));
        }

        auto& V = m_cameraController->getCamera().getViewMatrix();
        auto& P = m_cameraController->getCamera().getProjectionMatrix();

        m_transforms.MV = V * m_transforms.M;
        m_transforms.MVP = invertYaxis * P * m_transforms.MV;

        m_skybox->updateTransforms(invertYaxis * P, V);
    }

    void Scene::render()
    {
        m_renderer->addCopyAction([this](VkCommandBuffer& commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            m_transformsBuffer->updateDeviceBuffer(commandBuffer, m_renderer->getCurrentVirtualFrameIndex());
            m_particleParamsBuffer->updateDeviceBuffer(commandBuffer, m_renderer->getCurrentVirtualFrameIndex());

            m_skybox->updateDeviceBuffers(commandBuffer, m_renderer->getCurrentVirtualFrameIndex());
        });

        m_renderer->addDrawAction([this](VkCommandBuffer& commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psPipeline->getHandle());
            m_descriptorSetGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx));
            m_descriptorSetGroup.setDynamicOffset(1, m_particleParamsBuffer->getDynamicOffset(frameIdx));
            m_descriptorSetGroup.bind(commandBuffer, m_psPipeline->getPipelineLayout());
            
            m_vertexBufferGroup.bind(commandBuffer);
            vkCmdDraw(commandBuffer, particles, 1, 0, 0);

            m_skybox->draw(commandBuffer, m_renderer->getCurrentVirtualFrameIndex());

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
        m_fsQuadPipeline = std::make_unique<FullScreenQuadPipeline>(m_renderer, &m_renderer->getDefaultRenderPass());
        // Create descriptor set for the image
        m_sceneDescSetGroup =
        {
            m_fsQuadPipeline->allocateDescriptorSet(FullScreenQuadPipeline::DisplayedImage)
        };

        // Create a view to the render target
        m_sceneImageView = m_device->createImageView(m_scenePass->getColorAttachment(), VK_IMAGE_VIEW_TYPE_2D_ARRAY, m_scenePass->getColorFormat(), VK_IMAGE_ASPECT_COLOR_BIT, 0, VulkanRenderer::NumVirtualFrames);

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = m_sceneImageView;
        imageInfo.sampler     = m_linearClampSampler;
        m_sceneDescSetGroup.postImageUpdate(0, 0, VK_DESCRIPTOR_TYPE_SAMPLER, imageInfo);
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo);
        m_sceneDescSetGroup.flushUpdates(m_device);
    }
}