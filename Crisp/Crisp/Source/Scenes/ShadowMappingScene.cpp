#include "ShadowMappingScene.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "Core/InputDispatcher.hpp"
#include "IO/ImageFileBuffer.hpp"
#include "Camera/CameraController.hpp"

#include "Renderer/RenderPasses/ShadowPass.hpp"
#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/Pipelines/BlinnPhongPipeline.hpp"
#include "Renderer/Pipelines/ColorAndShadowPipeline.hpp"
#include "Renderer/Pipelines/ShadowMapPipeline.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Renderer/Pipelines/NormalMapPipeline.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanImage.hpp"
#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "vulkan/VulkanSampler.hpp"
#include "Renderer/Texture.hpp"

#include "Renderer/RenderPasses/VarianceShadowMapPass.hpp"
#include "Renderer/Pipelines/VarianceShadowMapPipeline.hpp"

#include "Renderer/RenderPasses/BlurPass.hpp"
#include "Renderer/Pipelines/BlurPipeline.hpp"

#include "Lights/DirectionalLight.hpp"

#include "Techniques/CascadedShadowMapper.hpp"
#include "Models/BoxVisualizer.hpp"
#include "Geometry/TriangleMesh.hpp"

#include "GUI/Form.hpp"
#include "ShadowMappingPanel.hpp"

#include "Models/Skybox.hpp"
#include "Geometry/MeshGeometry.hpp"

namespace crisp
{
    namespace
    {

    }

    ShadowMappingScene::ShadowMappingScene(VulkanRenderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_device(m_renderer->getDevice())
    {
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);
        //app->getWindow()->getInputDispatcher()->keyPressed.subscribe([this](int key, int mode)
        //{
        //    if (key == GLFW_KEY_R)
        //    {
        //        m_renderMode = m_renderMode == 1 ? 0 : 1;
        //    }
        //
        //    if (key == GLFW_KEY_V)
        //    {
        //        m_boxVisualizer->updateFrusta(m_shadowMapper.get(), m_cameraController.get());
        //    }
        //});

        m_transforms.resize(22);
        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame);
        m_transforms[0].M = glm::scale(glm::vec3{ 100.0f, 1.0f, 100.0f });
        m_transforms[1].M = glm::translate(glm::vec3{ 0.0f, 0.0f, -4.0f });
        for (int i = 2; i < 22; i++) {
            m_transforms[i].M = glm::translate(glm::vec3{ i * 0.4f - 4.0f, 2.0f, 0.0f }) *
                glm::scale(glm::vec3{0.2f, 3.0f, 0.2f});
        }

        DirectionalLight light(glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f)));
        m_lvpTransforms.resize(1);
        m_lvpTransforms[0] = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, -50.0f, 50.0f) * light.getViewMatrix();
        m_lvpBuffer = std::make_unique<UniformBuffer>(m_renderer, m_lvpTransforms.size() * sizeof(glm::mat4), BufferUpdatePolicy::PerFrame);

        m_shadowPass = std::make_unique<ShadowPass>(m_renderer, 1024, 1);
        m_shadowMapPipeline = std::make_unique<ShadowMapPipeline>(m_renderer, m_shadowPass.get());
        m_shadowMapDescGroup = { m_shadowMapPipeline->allocateDescriptorSet(0) };
        m_shadowMapDescGroup.postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        m_shadowMapDescGroup.postBufferUpdate(0, 1, m_lvpBuffer->getDescriptorInfo());
        m_shadowMapDescGroup.flushUpdates(m_renderer->getDevice());

        m_vsmLightTransforms.resize(2);
        m_vsmLightTransforms[0] = glm::lookAt(glm::vec3(0.0f, 5.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_vsmLightTransforms[1] = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 20.0f);
        m_vsmLightBuffer = std::make_unique<UniformBuffer>(m_renderer, m_vsmLightTransforms.size() * sizeof(glm::mat4), BufferUpdatePolicy::PerFrame);

        m_vsmPass = std::make_unique<VarianceShadowMapPass>(m_renderer, 1024);
        m_vsmPipeline = std::make_unique<VarianceShadowMapPipeline>(m_renderer, m_vsmPass.get());
        m_vsmDescSetGroup = { m_vsmPipeline->allocateDescriptorSet(0) };
        m_vsmDescSetGroup.postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        m_vsmDescSetGroup.postBufferUpdate(0, 1, m_vsmLightBuffer->getDescriptorInfo());
        m_vsmDescSetGroup.flushUpdates(m_device);

        m_nearestNeighborSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        m_linearClampSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);


        m_scenePass = std::make_unique<SceneRenderPass>(m_renderer);

        m_colorAndShadowPipeline = std::make_unique<ColorAndShadowPipeline>(m_renderer, m_scenePass.get());
        auto firstSet = m_colorAndShadowPipeline->allocateDescriptorSet(0);
        for (auto& desc : m_colorAndShadowDescGroups)
            desc = { firstSet, m_colorAndShadowPipeline->allocateDescriptorSet(1) };

        m_colorAndShadowDescGroups[0].postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        m_colorAndShadowDescGroups[0].postBufferUpdate(0, 1, m_lvpBuffer->getDescriptorInfo(0, sizeof(glm::mat4)));
        m_colorAndShadowDescGroups[0].postBufferUpdate(0, 2, m_cameraBuffer->getDescriptorInfo(0, sizeof(CameraParameters)));

        m_blurParameters.sigma = 2.3333f;
        m_blurParameters.radius = 7;
        m_blurPass = std::make_unique<BlurPass>(m_renderer, VK_FORMAT_R32G32_SFLOAT, VkExtent2D{ 1024, 1024 });
        m_blurPipeline = std::make_unique<BlurPipeline>(m_renderer, m_blurPass.get());

        for (int i = 0; i < VulkanRenderer::NumVirtualFrames; ++i)
        {
            m_blurDescGroups[i] = { m_blurPipeline->allocateDescriptorSet(0) };
            m_blurDescGroups[i].postImageUpdate(0, 0, { m_linearClampSampler->getHandle(), m_vsmPass->getRenderTargetView(0, i)->getHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            m_blurDescGroups[i].flushUpdates(m_device);
        }

        int frameIdx = 0;

        m_vertBlurPass = std::make_unique<BlurPass>(m_renderer, VK_FORMAT_R32G32_SFLOAT, VkExtent2D{ 1024, 1024 });
        m_vertBlurPipeline = std::make_unique<BlurPipeline>(m_renderer, m_vertBlurPass.get());

        frameIdx = 0;
        for (auto& descGroup : m_vertBlurDescGroups) {
            descGroup = { m_vertBlurPipeline->allocateDescriptorSet(0) };
            descGroup.postImageUpdate(0, 0, { m_linearClampSampler->getHandle(), m_blurPass->getRenderTargetView(0, frameIdx++)->getHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            descGroup.flushUpdates(m_device);
        }

        frameIdx = 0;
        for (auto& desc : m_colorAndShadowDescGroups) {
            desc.postImageUpdate(1, 0, { m_nearestNeighborSampler->getHandle(), m_shadowPass->getRenderTargetView(0, frameIdx)->getHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            desc.postImageUpdate(1, 1, { m_linearClampSampler->getHandle(), m_vertBlurPass->getRenderTargetView(0, frameIdx++)->getHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            desc.flushUpdates(m_device);
        }

        auto imageBuffer = std::make_unique<ImageFileBuffer>("Resources/Textures/brickwall.png");
        // create texture image
        VkExtent3D imageExtent = { imageBuffer->getWidth(), imageBuffer->getHeight(), 1u };
        auto byteSize = imageExtent.width * imageExtent.height * imageBuffer->getNumComponents() * sizeof(unsigned char);

        uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(imageExtent.width, imageExtent.height)))) + 1;

        m_normalMap = std::make_unique<Texture>(m_renderer, imageExtent, 1, mipLevels, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_normalMap->fill(imageBuffer->getData(), byteSize);
        m_normalMapView = m_normalMap->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, mipLevels);


        m_normalMapPipeline = std::make_unique<NormalMapPipeline>(m_renderer, m_scenePass.get());
        auto firstNormalMapSet = m_normalMapPipeline->allocateDescriptorSet(0);
        auto thirdNormalMapSet = m_normalMapPipeline->allocateDescriptorSet(2);
        for (auto& desc : m_normalMapDescGroups)
            desc = { firstNormalMapSet, m_normalMapPipeline->allocateDescriptorSet(1), thirdNormalMapSet };

        m_linearRepeatSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, static_cast<float>(mipLevels));
        m_normalMapDescGroups[0].postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        m_normalMapDescGroups[0].postBufferUpdate(0, 1, m_lvpBuffer->getDescriptorInfo(0, sizeof(glm::mat4)));
        m_normalMapDescGroups[0].postBufferUpdate(0, 2, m_cameraBuffer->getDescriptorInfo(0, sizeof(CameraParameters)));
        m_normalMapDescGroups[0].postImageUpdate(2, 0, m_normalMapView->getDescriptorInfo(m_linearRepeatSampler->getHandle()));

        frameIdx = 0;
        for (auto& desc : m_normalMapDescGroups) {
            desc.postImageUpdate(1, 0, m_shadowPass->getRenderTargetView(0, frameIdx)->getDescriptorInfo(m_nearestNeighborSampler->getHandle()));
            desc.postImageUpdate(1, 1, m_vertBlurPass->getRenderTargetView(0, frameIdx++)->getDescriptorInfo(m_linearClampSampler->getHandle()));
            desc.flushUpdates(m_device);
        }


        std::vector<glm::vec3> verts =
        {
            glm::vec3(-1.0f, 0.0f, 1.0f),   glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(+1.0f, 0.0f, 1.0f),   glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(+1.0f, 0.0f, -1.0f),  glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(-1.0f, 0.0f, -1.0f),  glm::vec3(0.0f, 1.0f, 0.0f)
        };

        std::vector<float> fullVerts =
        {
            -1.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
            +1.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
            +1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
            -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f
        };

        std::vector<glm::uvec3> faces =
        {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 2, 3)
        };
        std::vector<glm::uvec3> cwFaces =
        {
            glm::uvec3(0, 2, 1),
            glm::uvec3(0, 3, 2)
        };

        m_planeGeometry  = std::make_unique<MeshGeometry>(m_renderer, fullVerts, faces);
        m_cwPlaneGeometry = std::make_unique<MeshGeometry>(m_renderer, verts, cwFaces);

        auto posNormalFormat = { VertexAttribute::Position, VertexAttribute::Normal };
        m_sphereGeometry = std::make_unique<MeshGeometry>(m_renderer, "treeBase1.obj", posNormalFormat);
        m_cubeGeometry   = std::make_unique<MeshGeometry>(m_renderer, "cube.obj",   posNormalFormat);

        auto panel = std::make_unique<gui::ShadowMappingPanel>(app->getForm(), this);
        m_app->getForm()->add(std::move(panel));
        //m_transforms.resize(27);
        //for (uint32_t i = 0; i < 25; i++)
        //{
        //    float xTrans = (i / 5) * 5.0f;
        //    float zTrans = (i % 5) * 5.0f;
        //    m_transforms[i].M = glm::translate(glm::vec3(xTrans - 10.0f, 0.5f, zTrans - 10.0f));
        //}
        //m_transforms[25].M = glm::translate(glm::vec3(0.0f, -1.0f, 0.0f)) * glm::scale(glm::vec3(50.0f, 1.0f, 50.0f));
        //m_transforms[26].M = glm::translate(glm::vec3(2.5f, -1.0f, 0.0f)) * glm::rotate(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(3.0f));
        //m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(Transforms), BufferUpdatePolicy::PerFrame);
        //
        //uint32_t numCascades = 4;
        //DirectionalLight light(glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f)));
        //m_shadowMapper = std::make_unique<CascadedShadowMapper>(m_renderer, light, numCascades, m_transformsBuffer.get());
        //
        //
        //m_planeGeometry = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("plane.obj", { VertexAttribute::Position, VertexAttribute::Normal }));
        //m_sphereGeometry = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("sphere.obj", { VertexAttribute::Position, VertexAttribute::Normal }));
        //
        //m_nearestNeighborSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        //
        //m_scenePass = std::make_unique<SceneRenderPass>(m_renderer);
        //m_skybox = std::make_unique<Skybox>(m_renderer, m_scenePass.get());
        //
        //m_blinnPhongPipeline = std::make_unique<BlinnPhongPipeline>(m_renderer, m_scenePass.get());
        //auto firstSet = m_blinnPhongPipeline->allocateDescriptorSet(0);
        //for (auto& descGroup : m_blinnPhongDescGroups)
        //    descGroup = { firstSet, m_blinnPhongPipeline->allocateDescriptorSet(1) };
        //
        //m_blinnPhongDescGroups[0].postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(Transforms)));
        //m_blinnPhongDescGroups[0].postBufferUpdate(0, 1, m_shadowMapper->getLightTransformsBuffer()->getDescriptorInfo());
        //m_blinnPhongDescGroups[0].postBufferUpdate(0, 2, m_cameraBuffer->getDescriptorInfo());
        //for (uint32_t frameIdx = 0; frameIdx < VulkanRenderer::NumVirtualFrames; frameIdx++)
        //{
        //    for (uint32_t cascadeIdx = 0; cascadeIdx < numCascades; cascadeIdx++)
        //    {
        //        m_blinnPhongDescGroups[frameIdx].postImageUpdate(1, 0, cascadeIdx, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_nearestNeighborSampler->getHandle(), m_shadowMapper->getRenderPass()->getAttachmentView(cascadeIdx, frameIdx), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        //    }
        //    m_blinnPhongDescGroups[frameIdx].flushUpdates(m_device);
        //}
        //
        //m_bunnyGeometry = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("Nanosuit.obj", { VertexAttribute::Position, VertexAttribute::Normal }));
        //m_boxVisualizer = std::make_unique<BoxVisualizer>(m_renderer, 4, 4, m_scenePass.get());

        initRenderTargetResources();
    }

    ShadowMappingScene::~ShadowMappingScene()
    {
        m_app->getForm()->remove("shadowMappingPanel");
    }

    void ShadowMappingScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_scenePass->recreate();

        /*int frameIdx = 0;
        for (auto& descGroup : m_blurDescGroups) {
            descGroup.postImageUpdate(0, 0, { m_linearClampSampler->getHandle(), m_scenePass->getAttachmentView(0, frameIdx++), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            descGroup.flushUpdates(m_device);
        }

        frameIdx = 0;
        for (auto& descGroup : m_vertBlurDescGroups) {
            descGroup.postImageUpdate(0, 0, { m_linearClampSampler->getHandle(), m_blurPass->getAttachmentView(0, frameIdx++), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            descGroup.flushUpdates(m_device);
        }*/

        m_sceneImageView = m_scenePass->createRenderTargetView(0, VulkanRenderer::NumVirtualFrames);
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_sceneImageView->getDescriptorInfo());
        m_sceneDescSetGroup.flushUpdates(m_device);
    }

    void ShadowMappingScene::update(float dt)
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
            m_transformsBuffer->updateStagingBuffer(m_transforms.data(), m_transforms.size() * sizeof(TransformPack));
            m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

            m_lvpBuffer->updateStagingBuffer(m_lvpTransforms.data(), m_lvpTransforms.size() * sizeof(glm::mat4));

            m_vsmLightBuffer->updateStagingBuffer(m_vsmLightTransforms.data(), m_vsmLightTransforms.size() * sizeof(glm::mat4));

            //m_shadowMapper->recalculateLightProjections(m_cameraController->getCamera(), 50.0f, ParallelSplit::Logarithmic);

            //m_boxVisualizer->update(V, P);

            //m_skybox->updateTransforms(P, V);
        }
    }

    void ShadowMappingScene::render()
    {
        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            //m_skybox->updateDeviceBuffers(commandBuffer, frameIdx);

            m_transformsBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            m_cameraBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            m_lvpBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            m_vsmLightBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            //m_shadowMapper->update(commandBuffer, frameIdx);
            //m_boxVisualizer->updateDeviceBuffers(commandBuffer, frameIdx);
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
            //
            //std::vector<VkImageMemoryBarrier> barriers(4);
            //for (int i = 0; i < 4; i++) {
            //    barriers[i].sType = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            //    barriers[i].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            //    barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            //    barriers[i].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            //    barriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            //    barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            //    barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            //    barriers[i].image = m_shadowMapper->getShadowMap(i);
            //    barriers[i].subresourceRange.baseMipLevel = 0;
            //    barriers[i].subresourceRange.levelCount = 1;
            //    barriers[i].subresourceRange.baseArrayLayer = frameIdx;
            //    barriers[i].subresourceRange.layerCount = 1;
            //    barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            //}
            //
            //vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            //    0, 0, nullptr, 0, nullptr, barriers.size(), barriers.data());
        });

        m_renderer->enqueueDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();


            m_shadowPass->begin(commandBuffer);
            m_shadowMapPipeline->bind(commandBuffer);
            m_shadowMapDescGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 1 * sizeof(TransformPack));
            m_shadowMapDescGroup.setDynamicOffset(1, m_lvpBuffer->getDynamicOffset(frameIdx));
            m_shadowMapDescGroup.bind(commandBuffer, m_shadowMapPipeline->getPipelineLayout());
            m_shadowMapPipeline->setPushConstant(commandBuffer, VK_SHADER_STAGE_VERTEX_BIT, 0, 0);
            m_sphereGeometry->bindGeometryBuffers(commandBuffer);
            m_sphereGeometry->draw(commandBuffer);

            m_shadowMapDescGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx));
            m_shadowMapDescGroup.bind(commandBuffer, m_shadowMapPipeline->getPipelineLayout());
            m_cwPlaneGeometry->bindGeometryBuffers(commandBuffer);
            m_cwPlaneGeometry->draw(commandBuffer);


            m_shadowPass->end(commandBuffer);

            m_shadowPass->getRenderTarget(0)->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIdx, 1,
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            m_vsmPass->begin(commandBuffer);
            m_vsmPipeline->bind(commandBuffer);

            m_vsmDescSetGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx));
            m_vsmDescSetGroup.setDynamicOffset(1, m_vsmLightBuffer->getDynamicOffset(frameIdx));
            m_vsmDescSetGroup.bind(commandBuffer, m_vsmPipeline->getPipelineLayout());
            m_cwPlaneGeometry->bindGeometryBuffers(commandBuffer);
            m_cwPlaneGeometry->draw(commandBuffer);

            m_vsmDescSetGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 1 * sizeof(TransformPack));
            m_vsmDescSetGroup.setDynamicOffset(1, m_vsmLightBuffer->getDynamicOffset(frameIdx));
            m_vsmDescSetGroup.bind(commandBuffer, m_vsmPipeline->getPipelineLayout());
            m_sphereGeometry->bindGeometryBuffers(commandBuffer);
            m_sphereGeometry->draw(commandBuffer);

            m_cubeGeometry->bindGeometryBuffers(commandBuffer);
            for (int i = 2; i < 22; i++)
            {
                m_vsmDescSetGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + i * sizeof(TransformPack));
                m_vsmDescSetGroup.bind(commandBuffer, m_vsmPipeline->getPipelineLayout());
                m_cubeGeometry->draw(commandBuffer);
            }

            m_vsmPass->end(commandBuffer);

            m_vsmPass->getRenderTarget(0)->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIdx, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            m_blurPass->begin(commandBuffer);

            m_blurPipeline->bind(commandBuffer);
            m_blurDescGroups[frameIdx].bind(commandBuffer, m_blurPipeline->getPipelineLayout());

            m_blurParameters.texelSize.x = 1.0f / static_cast<float>(m_renderer->getSwapChainExtent().width);
            m_blurParameters.texelSize.y = 0.0f;
            m_blurPipeline->setPushConstant(commandBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, 0, m_blurParameters);

            m_renderer->drawFullScreenQuad(commandBuffer);

            m_blurPass->end(commandBuffer);

            m_blurPass->getRenderTarget(0)->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIdx, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            m_vertBlurPass->begin(commandBuffer);

            m_vertBlurPipeline->bind(commandBuffer);
            m_vertBlurDescGroups[frameIdx].bind(commandBuffer, m_vertBlurPipeline->getPipelineLayout());

            m_blurParameters.texelSize.x = 0.0f;
            m_blurParameters.texelSize.y = 1.0f / static_cast<float>(m_renderer->getSwapChainExtent().height);
            m_vertBlurPipeline->setPushConstant(commandBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, 0, m_blurParameters);

            m_renderer->drawFullScreenQuad(commandBuffer);

            m_vertBlurPass->end(commandBuffer);

            m_vertBlurPass->getRenderTarget(0)->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIdx, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            m_scenePass->begin(commandBuffer);
            m_colorAndShadowPipeline->bind(commandBuffer);
            m_colorAndShadowPipeline->setPushConstant(commandBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, 0, m_vsmLightTransforms[0]);
            m_colorAndShadowPipeline->setPushConstant(commandBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), m_vsmLightTransforms[1]);
            m_colorAndShadowDescGroups[frameIdx].setDynamicOffset(2, m_cameraBuffer->getDynamicOffset(frameIdx));
            m_colorAndShadowDescGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + sizeof(TransformPack));
            m_colorAndShadowDescGroups[frameIdx].bind(commandBuffer, m_colorAndShadowPipeline->getPipelineLayout());
            m_sphereGeometry->bindGeometryBuffers(commandBuffer);
            m_sphereGeometry->draw(commandBuffer);

            for (int i = 2; i < 22; i++)
            {
                m_colorAndShadowDescGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + i * sizeof(TransformPack));
                m_colorAndShadowDescGroups[frameIdx].bind(commandBuffer, m_colorAndShadowPipeline->getPipelineLayout());
                m_cubeGeometry->bindGeometryBuffers(commandBuffer);
                m_cubeGeometry->draw(commandBuffer);
            }

            m_normalMapPipeline->bind(commandBuffer);
            m_normalMapPipeline->setPushConstant(commandBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, 0, m_vsmLightTransforms[0]);
            m_normalMapPipeline->setPushConstant(commandBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), m_vsmLightTransforms[1]);
            m_normalMapDescGroups[frameIdx].setDynamicOffset(2, m_cameraBuffer->getDynamicOffset(frameIdx));
            m_normalMapDescGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx));
            m_normalMapDescGroups[frameIdx].bind(commandBuffer, m_normalMapPipeline->getPipelineLayout());
            m_planeGeometry->bindGeometryBuffers(commandBuffer);
            m_planeGeometry->draw(commandBuffer);

            //m_blinnPhongPipeline->bind(commandBuffer);
            //m_blinnPhongDescGroups[frameIdx].setDynamicOffset(1, m_shadowMapper->getLightTransformsBuffer()->getDynamicOffset(frameIdx));
            //m_blinnPhongDescGroups[frameIdx].setDynamicOffset(2, m_cameraBuffer->getDynamicOffset(frameIdx));
            //
            //for (uint32_t i = 0; i < 25; i++)
            //{
            //    m_blinnPhongDescGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + i * sizeof(Transforms));
            //    m_blinnPhongDescGroups[frameIdx].bind(commandBuffer, m_blinnPhongPipeline->getPipelineLayout());
            //    vkCmdPushConstants(commandBuffer, m_blinnPhongPipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(unsigned int), &m_renderMode);
            //    m_sphereGeometry->bindGeometryBuffers(commandBuffer);
            //    m_sphereGeometry->draw(commandBuffer);
            //}
            //
            //m_blinnPhongDescGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 25 * sizeof(Transforms));
            //m_blinnPhongDescGroups[frameIdx].bind(commandBuffer, m_blinnPhongPipeline->getPipelineLayout());
            //m_planeGeometry->bindGeometryBuffers(commandBuffer);
            //m_planeGeometry->draw(commandBuffer);
            //
            //m_blinnPhongDescGroups[frameIdx].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 26 * sizeof(Transforms));
            //m_blinnPhongDescGroups[frameIdx].bind(commandBuffer, m_blinnPhongPipeline->getPipelineLayout());
            //m_bunnyGeometry->bindGeometryBuffers(commandBuffer);
            //m_bunnyGeometry->draw(commandBuffer);
            //
            //m_boxVisualizer->render(commandBuffer, frameIdx);
            ////m_skybox->draw(commandBuffer, frameIdx);

            m_scenePass->end(commandBuffer);

            m_scenePass->getRenderTarget(0)->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIdx, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
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

    void ShadowMappingScene::setBlurRadius(int radius)
    {
        m_blurParameters.radius = radius;
        m_blurParameters.sigma = radius == 0 ? 1.0f : static_cast<float>(radius) / 3.0f;
    }

    void ShadowMappingScene::initRenderTargetResources()
    {
        //m_linearClampSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        m_fsQuadPipeline = std::make_unique<FullScreenQuadPipeline>(m_renderer, m_renderer->getDefaultRenderPass(), true);
        m_sceneDescSetGroup = { m_fsQuadPipeline->allocateDescriptorSet(FullScreenQuadPipeline::DisplayedImage) };

        m_sceneImageView = m_scenePass->createRenderTargetView(0, VulkanRenderer::NumVirtualFrames);
        m_sceneDescSetGroup.postImageUpdate(0, 0, VK_DESCRIPTOR_TYPE_SAMPLER, m_sceneImageView->getDescriptorInfo(m_linearClampSampler->getHandle()));
        m_sceneDescSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_sceneImageView->getDescriptorInfo());
        m_sceneDescSetGroup.flushUpdates(m_device);
    }
}