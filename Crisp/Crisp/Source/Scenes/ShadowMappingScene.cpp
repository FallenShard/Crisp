#include "ShadowMappingScene.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "IO/ImageFileBuffer.hpp"
#include "Camera/CameraController.hpp"

#include "Renderer/RenderPasses/ShadowPass.hpp"
#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/Pipelines/BlinnPhongPipelines.hpp"
#include "Renderer/Pipelines/ShadowMapPipelines.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Renderer/Pipelines/NormalMapPipeline.hpp"
#include "Renderer/Pipelines/UniformColorPipeline.hpp"
#include "Renderer/Pipelines/PhysicallyBasedPipeline.hpp"
#include "Renderer/Pipelines/GrassPipeline.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanImage.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "vulkan/VulkanSampler.hpp"
#include "Renderer/Texture.hpp"
#include "Renderer/Material.hpp"

#include "Renderer/Pipelines/LightShaftPipeline.hpp"

#include "Renderer/RenderPasses/VarianceShadowMapPass.hpp"

#include "Renderer/RenderPasses/BlurPass.hpp"
#include "Renderer/Pipelines/BlurPipeline.hpp"
#include "Renderer/RenderGraph.hpp"

#include "Renderer/RenderPasses/LightShaftPass.hpp"


#include "Lights/DirectionalLight.hpp"
#include "Lights/PointLight.hpp"

#include "Techniques/CascadedShadowMapper.hpp"
#include "Models/BoxVisualizer.hpp"
#include "Geometry/TriangleMesh.hpp"

#include "GUI/Form.hpp"
#include "ShadowMappingPanel.hpp"

#include "Models/Skybox.hpp"
#include "Geometry/Geometry.hpp"
#include "Geometry/MeshGenerators.hpp"
#include "Models/Grass.hpp"

#include <thread>

#include <CrispCore/Math/Constants.hpp>

#include <Core/LuaConfig.hpp>

#include "Geometry/VertexLayout.hpp"

namespace crisp
{
    namespace
    {
        template <typename ImageBuffer>
        std::unique_ptr<VulkanBuffer> createStagingBuffer(VulkanDevice* device, const ImageBuffer& imageBuffer)
        {
            auto stagingBuffer = std::make_unique<VulkanBuffer>(device, imageBuffer.getByteSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            stagingBuffer->updateFromHost(imageBuffer.getData(), imageBuffer.getByteSize(), 0);
            return stagingBuffer;
        }





        std::unique_ptr<VulkanImage> createTexture(Renderer* renderer, const ImageFileBuffer& imageFileBuffer, VkFormat format)
        {
            VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
            imageInfo.flags = 0;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.format = format;
            imageInfo.extent = { imageFileBuffer.getWidth(), imageFileBuffer.getHeight(), 1u };
            imageInfo.mipLevels = imageFileBuffer.getMipLevels();
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            auto image = std::make_unique<VulkanImage>(renderer->getDevice(), imageInfo, VK_IMAGE_ASPECT_COLOR_BIT);

            std::shared_ptr<VulkanBuffer> stagingBuffer = createStagingBuffer(renderer->getDevice(), imageFileBuffer);
            renderer->enqueueResourceUpdate([renderer, stagingBuffer, image = image.get()](VkCommandBuffer cmdBuffer)
            {
                image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                image->copyFrom(cmdBuffer, *stagingBuffer, 0, 1);

                if (image->getMipLevels() > 1)
                {
                    image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                    for (uint32_t i = 1; i < image->getMipLevels(); i++)
                    {
                        VkImageBlit imageBlit = {};

                        imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        imageBlit.srcSubresource.baseArrayLayer = 0;
                        imageBlit.srcSubresource.layerCount = 1;
                        imageBlit.srcSubresource.mipLevel = i - 1;
                        imageBlit.srcOffsets[1].x = int32_t(image->getWidth() >> (i - 1));
                        imageBlit.srcOffsets[1].y = int32_t(image->getHeight() >> (i - 1));
                        imageBlit.srcOffsets[1].z = 1;

                        imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        imageBlit.dstSubresource.baseArrayLayer = 0;
                        imageBlit.dstSubresource.layerCount = 1;
                        imageBlit.dstSubresource.mipLevel = i;
                        imageBlit.dstOffsets[1].x = int32_t(image->getWidth() >> i);
                        imageBlit.dstOffsets[1].y = int32_t(image->getHeight() >> i);
                        imageBlit.dstOffsets[1].z = 1;

                        VkImageSubresourceRange mipRange = {};
                        mipRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        mipRange.baseMipLevel = i;
                        mipRange.levelCount = 1;
                        mipRange.baseArrayLayer = 0;
                        mipRange.layerCount = 1;

                        image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                        vkCmdBlitImage(cmdBuffer, image->getHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            image->getHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
                        image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                    }
                }

                VkImageSubresourceRange mipRange = {};
                mipRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                mipRange.baseMipLevel = 0;
                mipRange.levelCount = image->getMipLevels();
                mipRange.baseArrayLayer = 0;
                mipRange.layerCount = 1;
                image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);


                renderer->scheduleBufferForRemoval(stagingBuffer);
            });

            return image;
        }

        std::unique_ptr<VulkanImage> createTexture(Renderer* renderer, const std::string& filename, VkFormat format, bool invertY = false)
        {
            ImageFileBuffer imageBuffer(renderer->getResourcesPath() / "Textures" / filename, 4, invertY);
            return createTexture(renderer, imageBuffer, format);
        }

        std::unique_ptr<VulkanImage> createEnvironmentMap(Renderer* renderer, const std::string& filename, VkFormat format, bool invertY = false)
        {
            HdrImageFileBuffer imageBuffer(renderer->getResourcesPath() / "Textures/EnvironmentMaps" / filename, 4, invertY);

            VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
            imageInfo.flags         = 0;
            imageInfo.imageType     = VK_IMAGE_TYPE_2D;
            imageInfo.format        = format;
            imageInfo.extent        = { imageBuffer.getWidth(), imageBuffer.getHeight(), 1u };
            imageInfo.mipLevels     = 1;
            imageInfo.arrayLayers   = 1;
            imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            auto image = std::make_unique<VulkanImage>(renderer->getDevice(), imageInfo, VK_IMAGE_ASPECT_COLOR_BIT);

            std::shared_ptr<VulkanBuffer> stagingBuffer = createStagingBuffer(renderer->getDevice(), imageBuffer);
            renderer->enqueueResourceUpdate([renderer, stagingBuffer, image = image.get()](VkCommandBuffer cmdBuffer)
            {
                image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                image->copyFrom(cmdBuffer, *stagingBuffer, 0, 1);

                //if (image->getMipLevels() > 1)
                //{
                //    image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                //    for (uint32_t i = 1; i < image->getMipLevels(); i++)
                //    {
                //        VkImageBlit imageBlit = {};
                //
                //        imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                //        imageBlit.srcSubresource.baseArrayLayer = 0;
                //        imageBlit.srcSubresource.layerCount = 1;
                //        imageBlit.srcSubresource.mipLevel = i - 1;
                //        imageBlit.srcOffsets[1].x = int32_t(image->getWidth() >> (i - 1));
                //        imageBlit.srcOffsets[1].y = int32_t(image->getHeight() >> (i - 1));
                //        imageBlit.srcOffsets[1].z = 1;
                //
                //        imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                //        imageBlit.dstSubresource.baseArrayLayer = 0;
                //        imageBlit.dstSubresource.layerCount = 1;
                //        imageBlit.dstSubresource.mipLevel = i;
                //        imageBlit.dstOffsets[1].x = int32_t(image->getWidth() >> i);
                //        imageBlit.dstOffsets[1].y = int32_t(image->getHeight() >> i);
                //        imageBlit.dstOffsets[1].z = 1;
                //
                //        VkImageSubresourceRange mipRange = {};
                //        mipRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                //        mipRange.baseMipLevel = i;
                //        mipRange.levelCount = 1;
                //        mipRange.baseArrayLayer = 0;
                //        mipRange.layerCount = 1;
                //
                //        image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                //        vkCmdBlitImage(cmdBuffer, image->getHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                //            image->getHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
                //        image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                //    }
                //}

                VkImageSubresourceRange mipRange = {};
                mipRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                mipRange.baseMipLevel   = 0;
                mipRange.levelCount     = 1;image->getMipLevels();
                mipRange.baseArrayLayer = 0;
                mipRange.layerCount     = 1;
                image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

                renderer->scheduleBufferForRemoval(stagingBuffer);
            });

            return image;
        }

        glm::vec3 lightWorldPos = 300.0f * glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));

        float planeTilt = 90.0f;
        float angle = glm::radians(90.0f);
        glm::vec4 pointLightPos = glm::vec4(5.0f * std::cosf(angle), 3.0f, 5.0f * std::sinf(angle), 1.0f);
        bool animate = false;

        struct Params
        {
            glm::vec4 lightPos;
            float roughness = 0.1f;
            float metallic = 0.0f;
        };

        Params params;
    }

    ShadowMappingScene::ShadowMappingScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
    {
        // Key Press Events
        m_app->getWindow()->getEventHub().keyPressed += [this](Key key, int)
        {
            if (key == Key::Space)
                animate = !animate;

            if (key == Key::Up)
            {
                planeTilt += 15.0f;
                m_transforms[0].M = glm::rotate(glm::radians(planeTilt), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(10.0f, 1.0f, 10.0f));
            }

            if (key == Key::Down)
            {
                planeTilt -= 15.0f;
                m_transforms[0].M = glm::rotate(glm::radians(planeTilt), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(10.0f, 1.0f, 10.0f));
            }
        };

        // Camera
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        m_renderGraph = std::make_unique<RenderGraph>(m_renderer);

        // Render passes
        auto& mainPassNode = m_renderGraph->addRenderPass("mainPass", std::make_unique<SceneRenderPass>(m_renderer));
        m_renderGraph->addDependency("mainPass", "SCREEN", [](const VulkanRenderPass& pass, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
        {
            pass.getRenderTarget(0)->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIndex, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
        m_renderer->setSceneImageView(mainPassNode.renderPass->createRenderTargetView(0, Renderer::NumVirtualFrames));

        uint32_t numCascades = 4;
        DirectionalLight light(glm::normalize(glm::vec3(-1.0f, -1.0f, 0.0f)), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(-30.0f, -30.0f, -50.0f), glm::vec3(30.0f, 30.0f, 50.0f));
        m_cascadedShadowMapper = std::make_unique<CascadedShadowMapper>(m_renderer, numCascades, light, 80.0f);
        auto& csmPassNode = m_renderGraph->addRenderPass("csmPass", std::make_unique<ShadowPass>(m_renderer, 2048, numCascades));
        m_renderGraph->addDependency("csmPass", "mainPass", [](const VulkanRenderPass& pass, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
        {
            std::vector<VkImageMemoryBarrier> barriers(4);
            for (uint32_t i = 0; i < pass.getNumSubpasses(); i++)
            {
                barriers[i].sType = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                barriers[i].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barriers[i].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                barriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barriers[i].image = pass.getRenderTarget(i)->getHandle();
                barriers[i].subresourceRange.baseMipLevel = 0;
                barriers[i].subresourceRange.levelCount = 1;
                barriers[i].subresourceRange.baseArrayLayer = frameIndex;
                barriers[i].subresourceRange.layerCount = 1;
                barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            }

            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, barriers.size(), barriers.data());
        });

        auto& lightPassNode = m_renderGraph->addRenderPass("lightPass", std::make_unique<LightShaftPass>(m_renderer));
        m_renderGraph->addDependency("lightPass", "mainPass", [](const VulkanRenderPass& pass, VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
            pass.getRenderTarget(0)->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIndex, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

        m_renderGraph->sortRenderPasses();


        m_transforms.resize(26);
        m_transforms[0].M = glm::rotate(glm::radians(planeTilt), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(10.0f, 1.0f, 10.0f));
        m_transforms[1].M = glm::scale(glm::vec3(0.05f)) * glm::rotate(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_transforms[2].M = glm::translate(glm::vec3(4.0f));
        m_transforms[3].M = glm::translate(glm::vec3(pointLightPos)) * glm::scale(glm::vec3(0.2f));
        m_transforms[4].M = glm::translate(glm::vec3(4.0f, 0.0f, 4.0f));
        m_transforms[5].M = glm::rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(10.0f, 1.0f, 10.0f));
        m_transforms[6].M = glm::mat4(1.0f);

        std::vector<glm::vec3> offsets = { {-3.0f, 0.0f, 2.5f}, {-3.0f, 0.0f, 3.0f }, {2.0f, 0.0f, -1.0f} };

        //for (int i = 0; i < 5; i++)
        //    for (int j = 0; j < 5; j++)
        //        m_transforms[i * 5 + j + 1].M = glm::translate(glm::vec3(10.0f * i, 0.0f, 10.0f * j) + offsets[(i + j) % 3]) * glm::scale(glm::vec3(1.2f));

        m_transforms.push_back(TransformPack());
        m_transforms.back().M = (glm::translate(lightWorldPos));

        m_transformBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        VulkanDevice* device = m_renderer->getDevice();
        m_csmPipelines.reserve(numCascades);
        for (uint32_t i = 0; i < numCascades; i++)
            m_csmPipelines.emplace_back(createShadowMapPipeline(m_renderer, csmPassNode.renderPass.get(), i));
        m_csmMaterial = std::make_unique<Material>(m_csmPipelines[0].get(), std::vector<uint32_t>{ 0 });
        m_csmMaterial->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        m_csmMaterial->writeDescriptor(0, 1, 0, m_cascadedShadowMapper->getLightTransformBuffer()->getDescriptorInfo());

        m_texture = createTexture(m_renderer, "container2.png", VK_FORMAT_R8G8B8A8_SRGB, true);
        m_textureView = m_texture->createView(VK_IMAGE_VIEW_TYPE_2D);
        m_linearClampSampler = std::make_unique<VulkanSampler>(device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 11.0f);
        m_colorPipeline = createDiffusePipeline(m_renderer, mainPassNode.renderPass.get());
        m_colorMaterial = std::make_unique<Material>(m_colorPipeline.get(), std::vector<uint32_t>{ 0, 1 });
        m_colorMaterial->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        m_colorMaterial->writeDescriptor(1, 0, 0, *m_textureView, m_linearClampSampler->getHandle());


        auto vertexFormat = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent };

        m_geometries.emplace_back(createGeometry(m_renderer, createPlaneMesh(vertexFormat)));
        m_geometries.emplace_back(createGeometry(m_renderer, createSphereMesh(vertexFormat)));
        m_geometries.emplace_back(createGeometry(m_renderer, createSphereMesh({VertexAttribute::Position})));
        m_geometries.emplace_back(createGeometry(m_renderer, m_renderer->getResourcesPath() / "Meshes/ShaderBall_FWVN_PosX.obj", vertexFormat));
        m_geometries.emplace_back(createGeometry(m_renderer, createCubeMesh({ VertexAttribute::Position })));
        LuaConfig config(m_renderer->getResourcesPath() / "Scripts/scene.lua");
        auto texFolder = config.get<std::string>("texture").value_or("RustedIron");

        createDefaultPbrTextures();

        m_normalMap = createTexture(m_renderer, "nm2.png", VK_FORMAT_R8G8B8A8_UNORM, true);
        m_normalMapView = m_normalMap->createView(VK_IMAGE_VIEW_TYPE_2D);

        m_envMap = createEnvironmentMap(m_renderer, "NewportLoft.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, true);
        m_envMapView = m_envMap->createView(VK_IMAGE_VIEW_TYPE_2D);

        m_colorLightPipeline = createColorPipeline(m_renderer, mainPassNode.renderPass.get());
        m_colorLightMaterial = std::make_unique<Material>(m_colorLightPipeline.get(), std::vector<uint32_t>{ 0 });
        m_colorLightMaterial->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));

        m_linearRepeatSampler = std::make_unique<VulkanSampler>(device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, 16.0f, 12.0f);
        m_physBasedPipeline = createPbrPipeline(m_renderer, mainPassNode.renderPass.get(), 3);
        auto pbrMaterial = createPbrMaterial(texFolder);
        m_materials.push_back(std::move(pbrMaterial));

        m_irradiancePipeline = createIrradiancePipeline(m_renderer, mainPassNode.renderPass.get());
        auto irradianceMaterial = std::make_unique<Material>(m_irradiancePipeline.get(), std::vector<uint32_t>{0});
        irradianceMaterial->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        irradianceMaterial->writeDescriptor(0, 1, 0, *m_envMapView, m_linearRepeatSampler->getHandle());
        m_materials.push_back(std::move(irradianceMaterial));

        m_physBasedUnifPipeline = createPbrUnifPipeline(m_renderer, mainPassNode.renderPass.get());
        auto pbrUnifMaterial = std::make_unique<Material>(m_physBasedUnifPipeline.get(), std::vector<uint32_t>{0});
        pbrUnifMaterial->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        pbrUnifMaterial->writeDescriptor(0, 1, 0, m_cameraBuffer->getDescriptorInfo());
        pbrUnifMaterial->addDynamicBufferInfo(*m_cameraBuffer, 0);
        m_materials.push_back(std::move(pbrUnifMaterial));

        m_skybox = std::make_unique<Skybox>(m_renderer, *mainPassNode.renderPass, "Forest");
        device->flushDescriptorUpdates();


        //PointLight pointLight(glm::vec3(100.0f), glm::vec3(5.0f, 5.0f, 0.0f), glm::normalize(glm::vec3(-3.0f, -1.0f, 0.0f)));
        //DirectionalLight light(glm::normalize(glm::vec3(-1.0f, -1.0f, 0.0f)), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(-15.0f, -15.0f, -50.0f), glm::vec3(15.0f, 15.0f, 50.0f));
        //m_lights.resize(1);
        //m_lights[0] = pointLight.createDescriptorData();
        //m_lightBuffer = std::make_unique<UniformBuffer>(m_renderer, m_lights.size() * sizeof(LightDescriptorData), BufferUpdatePolicy::PerFrame);
        //m_transforms[0].M = pointLight.createModelMatrix(0.05f);
        //
        //
        //auto& shadowPassNode = m_renderGraph->addRenderPass(std::make_unique<ShadowPass>(m_renderer, 2048, 1));
        //shadowPassNode.dependencies.push_back([](const VulkanRenderPass& pass, VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
        //    pass.getRenderTarget(0)->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIndex, 1,
        //        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        //});
        //
        //m_shadowMapPipeline = createShadowMapPipeline(m_renderer, shadowPassNode.renderPass.get(), 0);
        //m_shadowMapMaterial = std::make_unique<Material>(m_shadowMapPipeline.get(), std::vector<uint32_t>{ 0 });
        //m_device->postDescriptorWrite(m_shadowMapMaterial->makeDescriptorWrite(0, 0), m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //m_device->postDescriptorWrite(m_shadowMapMaterial->makeDescriptorWrite(0, 1), m_cascadedShadowMapper->getLightTransformBuffer()->getDescriptorInfo());

        //m_nearestNeighborSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        //m_colorShadowPipeline = createBlinnPhongShadowPipeline(m_renderer, mainPassNode.renderPass.get());
        //m_colorShadowMaterial = std::make_unique<Material>(m_colorShadowPipeline.get(), std::vector<uint32_t>{ 0 }, std::vector<uint32_t>{ 1 });
        //m_device->postDescriptorWrite(m_colorShadowMaterial->makeDescriptorWrite(0, 0), m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //m_device->postDescriptorWrite(m_colorShadowMaterial->makeDescriptorWrite(0, 1), m_cascadedShadowMapper->getLightTransformBuffer()->getDescriptorInfo());
        //m_device->postDescriptorWrite(m_colorShadowMaterial->makeDescriptorWrite(0, 2), m_cascadedShadowMapper->getSplitBuffer()->getDescriptorInfo());
        //for (uint32_t i = 0; i < Renderer::NumVirtualFrames; i++)
        //{
        //    for (int c = 0; c < 4; c++)
        //    {
        //        m_device->postDescriptorWrite(m_colorShadowMaterial->makeDescriptorWrite(1, 0, c, i), csmPassNode.renderPass->getRenderTargetView(c, i)->getDescriptorInfo(m_nearestNeighborSampler->getHandle()));
        //    }
        //
        //   //  m_device->postDescriptorWrite(m_colorShadowMaterial->makeDescriptorWrite(1, 0, i), csmPassNode.renderPass->getRenderTargetView(0, i)->getDescriptorInfo(m_nearestNeighborSampler->getHandle()));
        //}
        //
        //m_grass = std::make_unique<Grass>(m_renderer, mainPassNode.renderPass.get(), csmPassNode.renderPass.get(), m_cascadedShadowMapper.get(), m_cameraBuffer.get(), m_nearestNeighborSampler.get());
        //
        //
        //m_normalMap = createTexture(m_renderer, "nm2.png", true);
        //m_normalMapView = m_normalMap->createView(VK_IMAGE_VIEW_TYPE_2D);
        //m_normalMapPipeline = createNormalMapPipeline(m_renderer, mainPassNode.renderPass.get());
        //m_normalMapMaterial = std::make_unique<Material>(m_normalMapPipeline.get(), std::vector<uint32_t>{ 0, 1 }, std::vector<uint32_t>{ 2 });
        //m_linearRepeatSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, static_cast<float>(m_normalMap->getMipLevels()));
        //m_device->postDescriptorWrite(m_normalMapMaterial->makeDescriptorWrite(0, 0), m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //m_device->postDescriptorWrite(m_normalMapMaterial->makeDescriptorWrite(0, 1), m_cameraBuffer->getDescriptorInfo(0, sizeof(CameraParameters)));
        //m_device->postDescriptorWrite(m_normalMapMaterial->makeDescriptorWrite(1, 0), m_normalMapView->getDescriptorInfo(m_linearRepeatSampler->getHandle()));
        //
        //for (uint32_t i = 0; i < Renderer::NumVirtualFrames; i++)
        //{
        //    //m_device->postDescriptorWrite(m_normalMapMaterial->makeDescriptorWrite(2, 0, i), m_lightBuffer->getDescriptorInfo(0, sizeof(LightDescriptorData)));
        //    //m_device->postDescriptorWrite(m_normalMapMaterial->makeDescriptorWrite(2, 1, i), m_shadowPass->getRenderTargetView(0, i)->getDescriptorInfo(m_nearestNeighborSampler->getHandle()));
        //}
        //
        ////m_normalMapMaterial->postBufferUpdate(0, 1, m_lvpBuffer->getDescriptorInfo(0, sizeof(glm::mat4)));
        ////m_vsmLightTransforms.resize(2);
        ////m_vsmLightTransforms[0] = glm::lookAt(glm::vec3(0.0f, 5.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ////m_vsmLightTransforms[1] = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 20.0f);
        ////m_vsmLightBuffer = std::make_unique<UniformBuffer>(m_renderer, m_vsmLightTransforms.size() * sizeof(glm::mat4), BufferUpdatePolicy::PerFrame);
        //auto fullVertex = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent };
        //m_spherePosGeometry  = makeGeometry(m_renderer, "uv_sphere.obj", { VertexAttribute::Position });
        ////m_fullSphereGeometry = makeGeometry(m_renderer, "nanosuit.obj", fullVertex);
        //
        //auto multiMesh = loadMesh(m_renderer, "nanosuit/nanosuit.obj", fullVertex);
        //m_fullSphereGeometry = std::move(multiMesh.first);
        //m_geometryViews = std::move(multiMesh.second);
        //
        //auto parts = { "glass", "leg", "hand", "arm", "helmet", "body" };
        //for (auto& part : parts)
        //{
        //    std::string path = "nanosuit/";
        //    path += part;
        //    path += "_showroom_ddn.png";
        //    //path += "_dif.png";
        //    m_normalMaps.push_back(createTexture(m_renderer, path, true));
        //    m_normalMapViews.push_back(m_normalMaps.back()->createView(VK_IMAGE_VIEW_TYPE_2D));
        //
        //    auto material = std::make_unique<Material>(m_normalMapPipeline.get(), std::vector<uint32_t>{ 0, 1 }, std::vector<uint32_t>{ 2 });
        //    m_device->postDescriptorWrite(material->makeDescriptorWrite(0, 0), m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //    m_device->postDescriptorWrite(material->makeDescriptorWrite(0, 1), m_cameraBuffer->getDescriptorInfo(0, sizeof(CameraParameters)));
        //    m_device->postDescriptorWrite(material->makeDescriptorWrite(1, 0), m_normalMapViews.back()->getDescriptorInfo(m_linearRepeatSampler->getHandle()));
        //    for (uint32_t i = 0; i < Renderer::NumVirtualFrames; i++)
        //    {
        //        m_device->postDescriptorWrite(material->makeDescriptorWrite(2, 0, i), m_lightBuffer->getDescriptorInfo(0, sizeof(LightDescriptorData)));
        //        m_device->postDescriptorWrite(material->makeDescriptorWrite(2, 1, i), m_shadowPass->getRenderTargetView(0, i)->getDescriptorInfo(m_nearestNeighborSampler->getHandle()));
        //    }
        //    m_materials.push_back(std::move(material));
        //}
        //
        //
        ////m_fullSphereGeometry = makeGeometry(m_renderer, createCubeMesh({ VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent }));
        //
        ////m_fullSphereGeometry = makeGeometry(m_renderer, makeSphereMesh({ VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent }));
        //
        //m_colorLightPipeline = createColorPipeline(m_renderer, lightPassNode.renderPass.get());
        //m_colorLightMaterial = std::make_unique<Material>(m_colorLightPipeline.get(), std::vector<uint32_t>{ 0 });
        //m_device->postDescriptorWrite(m_colorLightMaterial->makeDescriptorWrite(0, 0), m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //

        //
        //
        //m_treeGeometry = createGeometry(m_renderer, m_renderer->getResourcesPath() / "Meshes/treeBase1.obj", { VertexAttribute::Position });
        //
        //m_sphereGeometry = createGeometry(m_renderer, m_renderer->getResourcesPath() / "Meshes/sphere.obj", { VertexAttribute::Position });
        //
        //m_linearClampSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        //m_lightShaftPipeline = createLightShaftPipeline(m_renderer, mainPassNode.renderPass.get());
        //m_lightShaftMaterial = std::make_unique<Material>(m_lightShaftPipeline.get(), std::vector<uint32_t>{}, std::vector<uint32_t>{ 0 });
        //
        //m_lightShaftParams.decay    = 1.0f;
        //m_lightShaftParams.exposure = 0.0534f;
        //m_lightShaftParams.density  = 0.34f;
        //m_lightShaftParams.weight   = 8.65f;
        //m_lightShaftBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(m_lightShaftParams), BufferUpdatePolicy::PerFrame);
        //m_lightShaftBuffer->updateStagingBuffer(m_lightShaftParams);
        //for (uint32_t i = 0; i < Renderer::NumVirtualFrames; i++)
        //{
        //    m_device->postDescriptorWrite(m_lightShaftMaterial->makeDescriptorWrite(0, 0, 0, i), lightPassNode.renderPass->getRenderTargetView(0, i)->getDescriptorInfo(m_linearClampSampler->getHandle()));
        //    m_device->postDescriptorWrite(m_lightShaftMaterial->makeDescriptorWrite(0, 1, 0, i), m_lightShaftBuffer->getDescriptorInfo());
        //}


        //{
        //    DrawCommand drawLight;
        //    drawLight.pipeline = m_colorLightPipeline.get();
        //    drawLight.material = m_colorLightMaterial.get();
        //    drawLight.dynamicBuffers.push_back({ *m_transformBuffer, ((uint32_t)m_transforms.size() - 1) * sizeof(TransformPack) });
        //    drawLight.setPushConstants(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        //    drawLight.geometry = m_sphereGeometry.get();
        //    drawLight.setGeometryView<IndexedGeometryView>(m_sphereGeometry->createIndexedGeometryView());
        //    lightPassNode.addCommand(std::move(drawLight));

        //    DrawCommand drawPlane;
        //    drawPlane.pipeline = m_colorLightPipeline.get();
        //    drawPlane.material = m_colorLightMaterial.get();
        //    drawPlane.dynamicBuffers.push_back({ *m_transformBuffer, 0 * sizeof(TransformPack) });
        //    drawPlane.setPushConstants(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        //    drawPlane.geometry = m_planeGeometry.get();
        //    drawPlane.setGeometryView<IndexedGeometryView>(m_planeGeometry->createIndexedGeometryView());
        //    //lightPassNode.addCommand(std::move(drawPlane));
        //    for (int i = 0; i < 25; i++)
        //    {
        //        DrawCommand drawTree;
        //        drawTree.pipeline = m_colorLightPipeline.get();
        //        drawTree.material = m_colorLightMaterial.get();
        //        drawTree.dynamicBuffers.push_back({ *m_transformBuffer, (i + 1) * sizeof(TransformPack) });
        //        drawTree.setPushConstants(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        //        drawTree.geometry = m_treeGeometry.get();
        //        drawTree.setGeometryView<IndexedGeometryView>(m_treeGeometry->createIndexedGeometryView());
        //        lightPassNode.addCommand(std::move(drawTree));
        //    }
        //}

        //DrawCommand drawLight;
        //drawLight.pipeline = m_colorPipeline.get();
        //drawLight.material = m_colorMaterial.get();
        //drawLight.dynamicBuffers.push_back({ *m_transformBuffer, ((uint32_t)m_transforms.size() - 1) * sizeof(TransformPack) });
        //drawLight.setPushConstants(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        //drawLight.geometry = m_sphereGeometry.get();
        //drawLight.setGeometryView<IndexedGeometryView>(m_sphereGeometry->createIndexedGeometryView());
        //mainPassNode.addCommand(std::move(drawLight));



        //DrawCommand drawPlane;
        //drawPlane.pipeline = m_colorShadowPipeline.get();
        //drawPlane.dynamicBuffers.push_back({ *m_transformBuffer, 0 * sizeof(TransformPack) });
        //drawPlane.dynamicBuffers.push_back({ *m_cascadedShadowMapper->getLightTransformBuffer(), 0 });
        //drawPlane.dynamicBuffers.push_back({ *m_cascadedShadowMapper->getSplitBuffer(), 0 });
        //drawPlane.material = m_colorShadowMaterial.get();
        //drawPlane.geometry = m_planeGeometry.get();
        //drawPlane.setGeometryView<IndexedGeometryView>(m_planeGeometry->createIndexedGeometryView());
        //mainPassNode.addCommand(std::move(drawPlane));

        //mainPassNode.addCommand(m_grass->createDrawCommand());

        //for (int i = 0; i < 25; i++)
        //{
        //    DrawCommand drawTree;
        //    drawTree.pipeline = m_colorShadowPipeline.get();
        //    drawTree.dynamicBuffers.push_back({ *m_transformBuffer, (i + 1) * sizeof(TransformPack) });
        //    drawTree.dynamicBuffers.push_back({ *m_cascadedShadowMapper->getLightTransformBuffer(), 0 });
        //    drawTree.dynamicBuffers.push_back({ *m_cascadedShadowMapper->getSplitBuffer(), 0 });
        //    drawTree.material = m_colorShadowMaterial.get();
        //    drawTree.geometry = m_treeGeometry.get();
        //    drawTree.setGeometryView<IndexedGeometryView>(m_treeGeometry->createIndexedGeometryView());
        //    mainPassNode.addCommand(std::move(drawTree));

        //    DrawCommand shadowTree;
        //    shadowTree.pipeline = m_shadowMapPipeline.get();
        //    shadowTree.dynamicBuffers.push_back({ *m_transformBuffer, (i + 1) * sizeof(TransformPack) });
        //    shadowTree.dynamicBuffers.push_back({ *m_cascadedShadowMapper->getLightTransformBuffer(), 0 });
        //    shadowTree.setPushConstants(0);
        //    shadowTree.material = m_shadowMapMaterial.get();
        //    shadowTree.geometry = m_treeGeometry.get();
        //    shadowTree.setGeometryView(m_treeGeometry->createIndexedGeometryView());
        //    shadowPassNode.addCommand(std::move(shadowTree));

        //    for (int c = 0; c < 4; c++)
        //    {
        //        DrawCommand csmTree;
        //        csmTree.pipeline = m_csmPipelines[c].get();
        //        csmTree.dynamicBuffers.push_back({ *m_transformBuffer, (i + 1) * sizeof(TransformPack) });
        //        csmTree.dynamicBuffers.push_back({ *m_cascadedShadowMapper->getLightTransformBuffer(), 0 });
        //        csmTree.setPushConstants(c);
        //        csmTree.material = m_csmMaterial.get();
        //        csmTree.geometry = m_treeGeometry.get();
        //        csmTree.setGeometryView(m_treeGeometry->createIndexedGeometryView());
        //        csmPassNode.addCommand(std::move(csmTree), c);

        //        csmPassNode.addCommand(m_grass->createShadowCommand(c), c);
        //    }
        //}



        //DrawCommand drawLightShafts;
        //drawLightShafts.pipeline = m_lightShaftPipeline.get();
        //drawLightShafts.material = m_lightShaftMaterial.get();
        //drawLightShafts.dynamicBuffers.push_back({ *m_lightShaftBuffer, 0 });
        //drawLightShafts.geometry = m_renderer->getFullScreenGeometry();
        //drawLightShafts.setGeometryView(m_renderer->getFullScreenGeometry()->createIndexedGeometryView());
        //
        //mainPassNode.addCommand(std::move(drawLightShafts));



        //m_shadowMapDescGroup = { m_shadowMapPipeline->allocateDescriptorSet(0) };
        //m_shadowMapDescGroup.postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //m_shadowMapDescGroup.postBufferUpdate(0, 1, m_lvpBuffer->getDescriptorInfo());
        //m_shadowMapDescGroup.flushUpdates(m_renderer->getDevice());
        //
        //m_vsmLightTransforms.resize(2);
        //m_vsmLightTransforms[0] = glm::lookAt(glm::vec3(0.0f, 5.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        //m_vsmLightTransforms[1] = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 20.0f);
        //m_vsmLightBuffer = std::make_unique<UniformBuffer>(m_renderer, m_vsmLightTransforms.size() * sizeof(glm::mat4), BufferUpdatePolicy::PerFrame);
        //
        //m_vsmPass = std::make_unique<VarianceShadowMapPass>(m_renderer, 1024);
        //m_vsmPipeline = std::make_unique<VarianceShadowMapPipeline>(m_renderer, m_vsmPass.get());
        //m_vsmDescSetGroup = { m_vsmPipeline->allocateDescriptorSet(0) };
        //m_vsmDescSetGroup.postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //m_vsmDescSetGroup.postBufferUpdate(0, 1, m_vsmLightBuffer->getDescriptorInfo());
        //m_vsmDescSetGroup.flushUpdates(m_device);
        //

        //m_linearClampSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        //
        //
        //m_scenePass = std::make_unique<SceneRenderPass>(m_renderer);
        //
        //m_colorAndShadowPipeline = std::make_unique<ColorAndShadowPipeline>(m_renderer, m_scenePass.get());
        //auto firstSet = m_colorAndShadowPipeline->allocateDescriptorSet(0);
        //for (auto& desc : m_colorAndShadowDescGroups)
        //    desc = { firstSet, m_colorAndShadowPipeline->allocateDescriptorSet(1) };
        //
        //m_colorAndShadowDescGroups[0].postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //m_colorAndShadowDescGroups[0].postBufferUpdate(0, 1, m_lvpBuffer->getDescriptorInfo(0, sizeof(glm::mat4)));
        //m_colorAndShadowDescGroups[0].postBufferUpdate(0, 2, m_cameraBuffer->getDescriptorInfo(0, sizeof(CameraParameters)));
        //
        //m_blurParameters.sigma = 2.3333f;
        //m_blurParameters.radius = 7;
        //m_blurPass = std::make_unique<BlurPass>(m_renderer, VK_FORMAT_R32G32_SFLOAT, VkExtent2D{ 1024, 1024 });
        //m_blurPipeline = std::make_unique<BlurPipeline>(m_renderer, m_blurPass.get());
        //
        //for (int i = 0; i < Renderer::NumVirtualFrames; ++i)
        //{
        //    m_blurDescGroups[i] = { m_blurPipeline->allocateDescriptorSet(0) };
        //    m_blurDescGroups[i].postImageUpdate(0, 0, { m_linearClampSampler->getHandle(), m_vsmPass->getRenderTargetView(0, i)->getHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        //    m_blurDescGroups[i].flushUpdates(m_device);
        //}
        //
        //int frameIdx = 0;
        //
        //m_vertBlurPass = std::make_unique<BlurPass>(m_renderer, VK_FORMAT_R32G32_SFLOAT, VkExtent2D{ 1024, 1024 });
        //m_vertBlurPipeline = std::make_unique<BlurPipeline>(m_renderer, m_vertBlurPass.get());
        //
        //frameIdx = 0;
        //for (auto& descGroup : m_vertBlurDescGroups) {
        //    descGroup = { m_vertBlurPipeline->allocateDescriptorSet(0) };
        //    descGroup.postImageUpdate(0, 0, { m_linearClampSampler->getHandle(), m_blurPass->getRenderTargetView(0, frameIdx++)->getHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        //    descGroup.flushUpdates(m_device);
        //}
        //
        //frameIdx = 0;
        //for (auto& desc : m_colorAndShadowDescGroups) {
        //    desc.postImageUpdate(1, 0, { m_nearestNeighborSampler->getHandle(), m_shadowPass->getRenderTargetView(0, frameIdx)->getHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        //    desc.postImageUpdate(1, 1, { m_linearClampSampler->getHandle(), m_vertBlurPass->getRenderTargetView(0, frameIdx++)->getHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        //    desc.flushUpdates(m_device);
        //}
        //
        //auto imageBuffer = std::make_unique<ImageFileBuffer>("Resources/Textures/brickwall.png");
        //// create texture image
        //VkExtent3D imageExtent = { imageBuffer->getWidth(), imageBuffer->getHeight(), 1u };
        //auto byteSize = imageExtent.width * imageExtent.height * imageBuffer->getNumComponents() * sizeof(unsigned char);
        //
        //uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(imageExtent.width, imageExtent.height)))) + 1;
        //
        //m_normalMap = std::make_unique<Texture>(m_renderer, imageExtent, 1, mipLevels, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        //m_normalMap->fill(imageBuffer->getData(), byteSize);
        //m_normalMapView = m_normalMap->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, mipLevels);
        //
        //
        //m_normalMapPipeline = std::make_unique<NormalMapPipeline>(m_renderer, m_scenePass.get());
        //m_normalMapMaterial = std::make_unique<Material>(m_normalMapPipeline.get(), std::vector<uint32_t>{ 0, 2 }, std::vector<uint32_t>{ 1 });
        ////auto firstNormalMapSet = m_normalMapPipeline->allocateDescriptorSet(0);
        ////auto thirdNormalMapSet = m_normalMapPipeline->allocateDescriptorSet(2);
        ////for (auto& desc : m_normalMapDescGroups)
        ////    desc = { firstNormalMapSet, m_normalMapPipeline->allocateDescriptorSet(1), thirdNormalMapSet };
        //
        //m_linearRepeatSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, static_cast<float>(mipLevels));
        //m_normalMapMaterial->postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //m_normalMapMaterial->postBufferUpdate(0, 1, m_lvpBuffer->getDescriptorInfo(0, sizeof(glm::mat4)));
        //m_normalMapMaterial->postBufferUpdate(0, 2, m_cameraBuffer->getDescriptorInfo(0, sizeof(CameraParameters)));
        //m_normalMapMaterial->postImageUpdate(2, 0, m_normalMapView->getDescriptorInfo(m_linearRepeatSampler->getHandle()));
        //
        //for (uint32_t frameIdx = 0; frameIdx < Renderer::NumVirtualFrames; frameIdx++)
        //{
        //    m_normalMapMaterial->postImageUpdate(frameIdx, 1, 0, m_shadowPass->getRenderTargetView(0, frameIdx)->getDescriptorInfo(m_nearestNeighborSampler->getHandle()));
        //    m_normalMapMaterial->postImageUpdate(frameIdx, 1, 1, m_vertBlurPass->getRenderTargetView(0, frameIdx)->getDescriptorInfo(m_linearClampSampler->getHandle()));
        //}
        //m_normalMapMaterial->flushUpdates(*m_device);
        //
        //std::vector<glm::vec3> verts =
        //{
        //    glm::vec3(-1.0f, 0.0f, 1.0f),   glm::vec3(0.0f, 1.0f, 0.0f),
        //    glm::vec3(+1.0f, 0.0f, 1.0f),   glm::vec3(0.0f, 1.0f, 0.0f),
        //    glm::vec3(+1.0f, 0.0f, -1.0f),  glm::vec3(0.0f, 1.0f, 0.0f),
        //    glm::vec3(-1.0f, 0.0f, -1.0f),  glm::vec3(0.0f, 1.0f, 0.0f)
        //};
        //
        //std::vector<float> fullVerts =
        //{
        //    -1.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
        //    +1.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
        //    +1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
        //    -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f
        //};
        //
        //std::vector<glm::uvec3> faces =
        //{
        //    glm::uvec3(0, 1, 2),
        //    glm::uvec3(0, 2, 3)
        //};
        //std::vector<glm::uvec3> cwFaces =
        //{
        //    glm::uvec3(0, 2, 1),
        //    glm::uvec3(0, 3, 2)
        //};
        //
        //m_planeGeometry   = std::make_unique<Geometry>(m_renderer, fullVerts, faces);
        //m_cwPlaneGeometry = std::make_unique<Geometry>(m_renderer, verts, cwFaces);
        //
        //auto posNormalFormat = { VertexAttribute::Position, VertexAttribute::Normal };
        //m_cubeGeometry     = makeGeometry(m_renderer, "cube.obj",      posNormalFormat);
        //m_treeBaseGeometry = makeGeometry(m_renderer, "treeBase1.obj", posNormalFormat);
        //
        auto panel = std::make_unique<gui::ShadowMappingPanel>(m_app->getForm(), this);
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

        //DirectionalLight light(glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f)));

        //
        //
        //m_planeGeometry = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("plane.obj", { VertexAttribute::Position, VertexAttribute::Normal }));
        //m_sphereGeometry = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("sphere.obj", { VertexAttribute::Position, VertexAttribute::Normal }));
        //
        //m_nearestNeighborSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        //
        //m_scenePass = std::make_unique<SceneRenderPass>(m_renderer);

        //
        //m_blinnPhongPipeline = std::make_unique<BlinnPhongPipeline>(m_renderer, m_scenePass.get());
        //auto firstSet = m_blinnPhongPipeline->allocateDescriptorSet(0);
        //for (auto& descGroup : m_blinnPhongDescGroups)
        //    descGroup = { firstSet, m_blinnPhongPipeline->allocateDescriptorSet(1) };
        //
        //m_blinnPhongDescGroups[0].postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(Transforms)));
        //m_blinnPhongDescGroups[0].postBufferUpdate(0, 1, m_shadowMapper->getLightTransformsBuffer()->getDescriptorInfo());
        //m_blinnPhongDescGroups[0].postBufferUpdate(0, 2, m_cameraBuffer->getDescriptorInfo());
        //for (uint32_t frameIdx = 0; frameIdx < Renderer::NumVirtualFrames; frameIdx++)
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
    }

    ShadowMappingScene::~ShadowMappingScene()
    {
        m_app->getForm()->remove("shadowMappingPanel");
    }

    void ShadowMappingScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode("mainPass").renderPass->createRenderTargetView(0, Renderer::NumVirtualFrames));

        ///*int frameIdx = 0;
        //for (auto& descGroup : m_blurDescGroups) {
        //    descGroup.postImageUpdate(0, 0, { m_linearClampSampler->getHandle(), m_scenePass->getAttachmentView(0, frameIdx++), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        //    descGroup.flushUpdates(m_device);
        //}
        //
        //frameIdx = 0;
        //for (auto& descGroup : m_vertBlurDescGroups) {
        //    descGroup.postImageUpdate(0, 0, { m_linearClampSampler->getHandle(), m_blurPass->getAttachmentView(0, frameIdx++), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        //    descGroup.flushUpdates(m_device);
        //}*/
    }

    void ShadowMappingScene::update(float dt)
    {
        static float t = 0;
        //m_lights[0].position.x = sin(t);
        //auto up = glm::vec3(0.0f, 1.0f, 0.0f);
        //auto right = glm::normalize(glm::cross(glm::vec3(m_lights[0].position), up));
        //up = glm::normalize(glm::cross(right, glm::vec3(m_lights[0].position)));
        //
        //auto position = glm::vec3(0.0f, 0.0f, 0.0f);
        //m_lights[0].V = glm::lookAt(position, position + glm::vec3(m_lights[0].position), up);
        //m_lights[0].VP = m_lights[0].P * m_lights[0].V;
        //m_lvpTransforms[0] = light.getProjectionMatrix() * light.getViewMatrix() * glm::translate(glm::vec3(sinf(t), 0.0f, 0.0f));

        //m_lvpBuffer->updateStagingBuffer(m_lvpTransforms.data(), m_lvpTransforms.size() * sizeof(glm::mat4));
        //m_lightBuffer->updateStagingBuffer(m_lights.data(), m_lights.size() * sizeof(LightDescriptorData));

        t += dt;

        if (animate)
        {
            angle += 2.0f * PI * dt / 5.0f;
            pointLightPos.x = 5.0f * std::cosf(angle);
            pointLightPos.z = 5.0f * std::sinf(angle);
            m_transforms[3].M = glm::translate(glm::vec3(pointLightPos)) * glm::scale(glm::vec3(0.2f));
        }

        //float angle = glm::radians(t * 2.0f);
        //glm::mat4 rotMat = glm::rotate(angle, glm::vec3(0.0f, 0.0f, 1.0f));
        //
        //glm::vec3 dir = rotMat * glm::vec4(glm::normalize(glm::vec3(-1.0f, -1.0f, 0.0f)), 0.0f);
        //light.setDirection(glm::normalize(dir));
        //
        //updateCsm();

        auto viewChanged = m_cameraController->update(dt);
        //if (viewChanged)
        {
            auto& V = m_cameraController->getViewMatrix();
            auto& P = m_cameraController->getProjectionMatrix();

            for (auto& trans : m_transforms)
            {
                trans.MV  = V * trans.M;
                trans.MVP = P * trans.MV;
                trans.N = glm::transpose(glm::inverse(glm::mat3(trans.MV)));
            }

            m_transformBuffer->updateStagingBuffer(m_transforms.data(), m_transforms.size() * sizeof(TransformPack));
            m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));


            //
            //m_vsmLightBuffer->updateStagingBuffer(m_vsmLightTransforms.data(), m_vsmLight Transforms.size() * sizeof(glm::mat4));

            //m_boxVisualizer->update(V, P);

            auto screen = m_renderer->getSwapChainExtent();

            glm::vec3 pt = glm::project(lightWorldPos, V, P, glm::vec4(0.0f, 0.0f, screen.width, screen.height));
            pt.x /= screen.width;
            pt.y /= screen.height;
            //m_lightShaftParams.lightPos = glm::vec2(pt);
            //m_lightShaftBuffer->updateStagingBuffer(m_lightShaftParams);

            m_cascadedShadowMapper->recalculateLightProjections(m_cameraController->getCamera());
            m_skybox->updateTransforms(V, P);
        }
    }

    void ShadowMappingScene::render()
    {
        m_renderGraph->clearCommandLists();

        auto& mainPass = m_renderGraph->getNode("mainPass");

        auto irrCommand = [this](Material* material, Geometry* geometry, int transformIndex)
        {
            DrawCommand drawCommand;
            drawCommand.pipeline = material->getPipeline();
            drawCommand.material = material;
            drawCommand.dynamicBuffers.push_back({ *m_transformBuffer, transformIndex * sizeof(TransformPack) });
            for (const auto& info : material->getDynamicBufferInfos())
                drawCommand.dynamicBuffers.push_back(info);
            drawCommand.geometry = geometry;
            drawCommand.setGeometryView(geometry->createIndexedGeometryView());
            return drawCommand;
        };


        int index = 1;
        mainPass.addCommand(createDrawCommand(m_materials[0].get(), m_geometries[1].get(), 6));
        mainPass.addCommand(irrCommand(m_materials[1].get(), m_geometries[4].get(), 2));
        //mainPass.addCommand(createDrawCommand(m_materials[0].get(), m_geometries[3].get(), index));
        //mainPass.addCommand(createDrawCommand(m_materials[0].get(), m_geometries[index].get(), 2));
        //mainPass.addCommand(createPbrUnifDrawCommand(m_materials[1].get(), m_geometries[index].get(), 4));
        mainPass.addCommand(createLightDrawCommand(m_colorLightMaterial.get(), m_geometries[2].get(), 3));
        mainPass.addCommand(m_skybox->createDrawCommand());

        m_renderGraph->executeDrawCommands();
    }

    void ShadowMappingScene::setBlurRadius(int radius)
    {
        //m_blurParameters.radius = radius;
        //m_blurParameters.sigma = radius == 0 ? 1.0f : static_cast<float>(radius) / 3.0f;
    }

    void ShadowMappingScene::setSplitLambda(double lambda)
    {
        m_cascadedShadowMapper->setSplitLambda(static_cast<float>(lambda), m_cameraController->getCamera());
    }

    void ShadowMappingScene::setMetallic(double metallic)
    {
        params.metallic = metallic;
    }

    void ShadowMappingScene::setRoughness(double roughness)
    {
        params.roughness = roughness;
    }

    DrawCommand ShadowMappingScene::createDrawCommand(Material* material, Geometry* geometry, int transformIndex)
    {
        DrawCommand drawCommand;
        drawCommand.pipeline = material->getPipeline();
        drawCommand.material = material;
        drawCommand.dynamicBuffers.push_back({ *m_transformBuffer, transformIndex * sizeof(TransformPack) });
        for (const auto& info : material->getDynamicBufferInfos())
            drawCommand.dynamicBuffers.push_back(info);
        drawCommand.setPushConstants(pointLightPos);
        drawCommand.geometry = geometry;
        drawCommand.setGeometryView(geometry->createIndexedGeometryView());
        return drawCommand;
    }

    DrawCommand ShadowMappingScene::createLightDrawCommand(Material* material, Geometry* geometry, int transformIndex)
    {
        DrawCommand drawCommand = createDrawCommand(material, geometry, transformIndex);
        drawCommand.setPushConstants(glm::vec4(1.0f));
        return drawCommand;
    }

    DrawCommand ShadowMappingScene::createPbrUnifDrawCommand(Material* material, Geometry* geometry, int transformIndex)
    {
        DrawCommand drawCommand = createDrawCommand(material, geometry, transformIndex);
        params.lightPos = pointLightPos;
        drawCommand.setPushConstants(params);
        return drawCommand;
    }

    void ShadowMappingScene::createDefaultPbrTextures()
    {
        // Diffuse
        m_images.emplace_back(createTexture(m_renderer, ImageFileBuffer(1, 1, 4, { 255, 0, 255, 255 }), VK_FORMAT_R8G8B8A8_SRGB));
        m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));

        // Metalness
        m_images.emplace_back(createTexture(m_renderer, ImageFileBuffer(1, 1, 4, { 0, 0, 0, 255 }), VK_FORMAT_R8G8B8A8_UNORM));
        m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));

        // Roughness
        m_images.emplace_back(createTexture(m_renderer, ImageFileBuffer(1, 1, 4, { 32, 32, 32, 255 }), VK_FORMAT_R8G8B8A8_UNORM));
        m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));

        // Normal
        m_images.emplace_back(createTexture(m_renderer, ImageFileBuffer(1, 1, 4, { 127, 127, 255, 255 }), VK_FORMAT_R8G8B8A8_UNORM));
        m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));

        // Ambient occlusion
        m_images.emplace_back(createTexture(m_renderer, ImageFileBuffer(1, 1, 4, { 255, 255, 255, 255 }), VK_FORMAT_R8G8B8A8_UNORM));
        m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));
    }

    std::unique_ptr<Material> ShadowMappingScene::createPbrMaterial(const std::string& type)
    {
        auto material = std::make_unique<Material>(m_physBasedPipeline.get(), std::vector<uint32_t>{ 0 });
        material->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        material->writeDescriptor(0, 1, 0, m_cameraBuffer->getDescriptorInfo());
        material->addDynamicBufferInfo(*m_cameraBuffer, 0);

        const std::vector<std::string> texNames = { "diffuse", "metallic", "roughness", "normal", "ao" };
        const std::vector<VkFormat> formats = { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
        for (size_t i = 0; i < texNames.size(); ++i)
        {
            const std::string& filename = "PbrMaterials/" + type + "/" + texNames[i] + ".png";
            const auto& path = m_renderer->getResourcesPath() / "Textures" / filename;
            if (std::filesystem::exists(path))
            {
                m_images.emplace_back(createTexture(m_renderer, filename, formats[i], true));
                m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));
                material->writeDescriptor(0, 2 + i, 0, *m_imageViews.back(), m_linearRepeatSampler->getHandle());
            }
            else
            {
                logWarning("Texture type {} is using default values for '{}'\n", type, texNames[i]);
                material->writeDescriptor(0, 2 + i, 0, *m_imageViews[i], m_linearRepeatSampler->getHandle());
            }
        }

        material->writeDescriptor(0, 7, 0, *m_envMapView, m_linearRepeatSampler->getHandle());
        return material;
    }
}