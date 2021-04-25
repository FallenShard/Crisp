#include "OceanScene.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"

#include "Camera/CameraController.hpp"
#include "Renderer/ResourceContext.hpp"
#include "Renderer/RenderGraph.hpp"
#include "Renderer/RenderPasses/ForwardLightingPass.hpp"

#include "Geometry/Geometry.hpp"
#include "Geometry/TriangleMesh.hpp"
#include "Geometry/MeshGenerators.hpp"
#include "Renderer/VulkanImageUtils.hpp"

#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Lights/EnvironmentLighting.hpp"
#include "Core/LuaConfig.hpp"

#include <random>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace
{
    auto logger = spdlog::stdout_color_st("OceanScene");
}

namespace crisp
{
    namespace
    {
        static constexpr const char* MainPass = "mainPass";
        static constexpr const char* OscillationPass = "oscillationPass";

        constexpr int N = 2048;
        const int logN = std::log2(N);
        constexpr float Lx = 2.0f * N;

        std::unique_ptr<VulkanPipeline> createComputePipeline(Renderer* renderer, const glm::uvec3& workGroupSize, const PipelineLayoutBuilder& layoutBuilder, const std::string& shaderName)
        {
            VulkanDevice* device = renderer->getDevice();
            auto layout = layoutBuilder.create(device);

            std::vector<VkSpecializationMapEntry> specEntries =
            {
                //   id,               offset,             size
                { 0, 0 * sizeof(uint32_t), sizeof(uint32_t) },
                { 1, 1 * sizeof(uint32_t), sizeof(uint32_t) },
                { 2, 2 * sizeof(uint32_t), sizeof(uint32_t) }
            };

            VkSpecializationInfo specInfo = {};
            specInfo.mapEntryCount = static_cast<uint32_t>(specEntries.size());
            specInfo.pMapEntries   = specEntries.data();
            specInfo.dataSize      = sizeof(workGroupSize);
            specInfo.pData         = glm::value_ptr(workGroupSize);

            VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
            pipelineInfo.stage                     = createShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, renderer->getShaderModule(shaderName));
            pipelineInfo.stage.pSpecializationInfo = &specInfo;
            pipelineInfo.layout                    = layout->getHandle();
            pipelineInfo.basePipelineHandle        = VK_NULL_HANDLE;
            pipelineInfo.basePipelineIndex         = -1;
            VkPipeline pipeline;
            vkCreateComputePipelines(device->getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

            auto uniqueHandle = std::make_unique<VulkanPipeline>(device, pipeline, std::move(layout), PipelineDynamicStateFlags());
            uniqueHandle->setBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE);
            return uniqueHandle;
        }

        std::unique_ptr<VulkanPipeline> createOscillationPipeline(Renderer* renderer, const glm::uvec3& workGroupSize)
        {
            PipelineLayoutBuilder layoutBuilder;
            layoutBuilder.defineDescriptorSet(0, false,
                {
                    { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT },
                    { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT },
                    { 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT },
                    { 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT },
                    { 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT },
                    { 5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT }
                });

            layoutBuilder.addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(int32_t) + 3 * sizeof(float));
            return createComputePipeline(renderer, workGroupSize, layoutBuilder, "ocean-oscillate.comp");
        }

        std::unique_ptr<VulkanPipeline> createBitRevPipeline(Renderer* renderer, const glm::uvec3& workGroupSize)
        {
            PipelineLayoutBuilder layoutBuilder;
            layoutBuilder.defineDescriptorSet(0, false,
                {
                    { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1, VK_SHADER_STAGE_COMPUTE_BIT },
                    { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1, VK_SHADER_STAGE_COMPUTE_BIT },
                });

            layoutBuilder.addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(uint32_t) + sizeof(float));
            return createComputePipeline(renderer, workGroupSize, layoutBuilder, "ocean-fft-phillips.comp");
        }

        std::unique_ptr<VulkanPipeline> createTrueFFTPipeline(Renderer* renderer, const glm::uvec3& workGroupSize, const std::string& shaderName)
        {
            PipelineLayoutBuilder layoutBuilder;
            layoutBuilder.defineDescriptorSet(0, false,
                {
                    { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT },
                    { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT },
                });

            layoutBuilder.addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(int32_t) + sizeof(float));
            return createComputePipeline(renderer, workGroupSize, layoutBuilder, shaderName);
        }

        std::unique_ptr<VulkanImage> createStorageImage(VulkanDevice* device, uint32_t layerCount, uint32_t width, uint32_t height, VkFormat format)
        {
            VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
            createInfo.flags         = 0;
            createInfo.imageType     = VK_IMAGE_TYPE_2D;
            createInfo.format        = format;
            createInfo.extent        = { width, height, 1u };
            createInfo.mipLevels     = 1;
            createInfo.arrayLayers   = layerCount;
            createInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
            createInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
            createInfo.usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            return std::make_unique<VulkanImage>(device, createInfo, VK_IMAGE_ASPECT_COLOR_BIT);
        }


        bool paused = false;
    }

    OceanScene::OceanScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_resourceContext(std::make_unique<ResourceContext>(renderer))
        , m_renderGraph(std::make_unique<RenderGraph>(renderer))
    {
        m_app->getWindow()->keyPressed += [this](Key key, int modifiers) {
            if (key == Key::Space)
                paused = !paused;
            if (key == Key::F5)
                m_resourceContext->recreatePipelines();
        };

        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        std::vector<VertexAttributeDescriptor> vertexFormat = { VertexAttribute::Position };
        TriangleMesh mesh = createPlaneMesh(vertexFormat, 50.0, N - 1);
        m_resourceContext->addGeometry("ocean", std::make_unique<Geometry>(m_renderer, mesh, vertexFormat, false, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));


        auto fftImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
        auto displacementXImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
        auto displacementZImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
        auto normalXImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
        auto normalZImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
        auto spectrumImage = createInitialSpectrum();


        m_renderer->enqueueResourceUpdate([fftImg = fftImage.get(), this, dx = displacementXImage.get(), dz = displacementZImage.get(), nx = normalXImage.get(), nz = normalZImage.get()](VkCommandBuffer cmdBuffer)
        {
            fftImg->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            dx->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            dz->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            nx->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            nz->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });

        m_resourceContext->addImageView("fftImageView0", fftImage->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1));
        m_resourceContext->addImageView("fftImageView1", fftImage->createView(VK_IMAGE_VIEW_TYPE_2D, 1, 1));
        m_resourceContext->addImage("fftImage", std::move(fftImage));

        m_resourceContext->addImageView("displacementXView0", displacementXImage->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1));
        m_resourceContext->addImageView("displacementXView1", displacementXImage->createView(VK_IMAGE_VIEW_TYPE_2D, 1, 1));
        m_resourceContext->addImage("displacementX", std::move(displacementXImage));

        m_resourceContext->addImageView("displacementZView0", displacementZImage->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1));
        m_resourceContext->addImageView("displacementZView1", displacementZImage->createView(VK_IMAGE_VIEW_TYPE_2D, 1, 1));
        m_resourceContext->addImage("displacementZ", std::move(displacementZImage));

        m_resourceContext->addImageView("normalXView0", normalXImage->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1));
        m_resourceContext->addImageView("normalXView1", normalXImage->createView(VK_IMAGE_VIEW_TYPE_2D, 1, 1));
        m_resourceContext->addImage("normalX", std::move(normalXImage));

        m_resourceContext->addImageView("normalZView0", normalZImage->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1));
        m_resourceContext->addImageView("normalZView1", normalZImage->createView(VK_IMAGE_VIEW_TYPE_2D, 1, 1));
        m_resourceContext->addImage("normalZ", std::move(normalZImage));

        m_resourceContext->addImageView("randImageView", spectrumImage->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1));
        m_resourceContext->addImage("randImage", std::move(spectrumImage));

        auto& oscillationPass = m_renderGraph->addComputePass(OscillationPass);
        oscillationPass.workGroupSize = glm::ivec3(16, 16, 1);
        oscillationPass.numWorkGroups = glm::ivec3(N / 16, N / 16, 1);
        oscillationPass.pipeline = createOscillationPipeline(m_renderer, oscillationPass.workGroupSize);
        oscillationPass.material = std::make_unique<Material>(oscillationPass.pipeline.get());
        oscillationPass.material->writeDescriptor(0, 0, m_resourceContext->getImageView("randImageView")->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        oscillationPass.material->writeDescriptor(0, 1, m_resourceContext->getImageView("fftImageView0")->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        oscillationPass.material->writeDescriptor(0, 2, m_resourceContext->getImageView("displacementXView0")->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        oscillationPass.material->writeDescriptor(0, 3, m_resourceContext->getImageView("displacementZView0")->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        oscillationPass.material->writeDescriptor(0, 4, m_resourceContext->getImageView("normalXView0")->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        oscillationPass.material->writeDescriptor(0, 5, m_resourceContext->getImageView("normalZView0")->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

        oscillationPass.preDispatchCallback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIndex)
        {
            auto& pass = m_renderGraph->getNode(OscillationPass);

            VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer()->getHandle();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 0, nullptr, 1, &barrier, 0, nullptr);

            struct PCData
            {
                float time;
                int32_t numN;
                int32_t numM;
                float Lx;
                float Lz;
            };

            PCData values = { m_time, N, N, Lx, Lx };
            pass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, values);
        };

        int layerToRead = applyFFT("fftImage");
        int ltr1 = applyFFT("displacementX");
        int ltr2 = applyFFT("displacementZ");

        int ln1 = applyFFT("normalX");
        int ln2 = applyFFT("normalZ");

        m_renderGraph->addRenderPass(MainPass, std::make_unique<ForwardLightingPass>(m_renderer));

        m_renderGraph->addDependency(OscillationPass, MainPass, [this](const VulkanRenderPass&, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
        {
            VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer()->getHandle();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                0, 0, nullptr, 1, &barrier, 0, nullptr);
        });

        m_renderGraph->addRenderTargetLayoutTransition(MainPass, "SCREEN", 0);
        m_renderGraph->sortRenderPasses();
        m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);

        m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 200);

        m_resourceContext->addSampler("linearRepeat", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f));
        m_resourceContext->addSampler("linearClamp", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 11.0f));
        m_resourceContext->addSampler("linearMipmap", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 5.0f));

        // Environment map
        LuaConfig config(m_renderer->getResourcesPath() / "Scripts/scene.lua");
        auto hdrName = config.get<std::string>("environmentMap").value_or("GreenwichPark") + ".hdr";
        auto envRefMap = createEnvironmentMap(m_renderer, hdrName, VK_FORMAT_R32G32B32A32_SFLOAT, true);
        std::shared_ptr<VulkanImageView> envRefMapView = envRefMap->createView(VK_IMAGE_VIEW_TYPE_2D);

        auto [cubeMap, cubeMapView] = convertEquirectToCubeMap(m_renderer, envRefMapView, 1024);

        auto [diffEnv, diffEnvView] = setupDiffuseEnvMap(m_renderer, *cubeMapView, 64);
        m_resourceContext->addImageWithView("envIrrMap", std::move(diffEnv), std::move(diffEnvView));
        auto [reflEnv, reflEnvView] = setupReflectEnvMap(m_renderer, *cubeMapView, 512);
        m_resourceContext->addImageWithView("filteredMap", std::move(reflEnv), std::move(reflEnvView));

        m_resourceContext->addImageWithView("cubeMap", std::move(cubeMap), std::move(cubeMapView));
        m_resourceContext->addImageWithView("brdfLut", integrateBrdfLut(m_renderer));

        m_renderer->flushResourceUpdates(true);

        VulkanPipeline* pipeline = m_resourceContext->createPipeline("ocean", "ocean.lua", m_renderGraph->getRenderPass(MainPass), 0);
        Material* material = m_resourceContext->createMaterial("ocean", pipeline);
        material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        material->writeDescriptor(0, 1, m_resourceContext->getImageView("fftImageView" + std::to_string(layerToRead))->getDescriptorInfo(m_resourceContext->getSampler("linearRepeat"),
            VK_IMAGE_LAYOUT_GENERAL));
        material->writeDescriptor(0, 2, m_resourceContext->getImageView("displacementXView" + std::to_string(ltr1))->getDescriptorInfo(m_resourceContext->getSampler("linearRepeat"),
            VK_IMAGE_LAYOUT_GENERAL));
        material->writeDescriptor(0, 3, m_resourceContext->getImageView("displacementZView" + std::to_string(ltr2))->getDescriptorInfo(m_resourceContext->getSampler("linearRepeat"),
            VK_IMAGE_LAYOUT_GENERAL));

        material->writeDescriptor(0, 4, m_resourceContext->getImageView("normalXView" + std::to_string(ln1))->getDescriptorInfo(m_resourceContext->getSampler("linearRepeat"),
            VK_IMAGE_LAYOUT_GENERAL));
        material->writeDescriptor(0, 5, m_resourceContext->getImageView("normalZView" + std::to_string(ln2))->getDescriptorInfo(m_resourceContext->getSampler("linearRepeat"),
            VK_IMAGE_LAYOUT_GENERAL));
        material->writeDescriptor(1, 0, *m_resourceContext->getUniformBuffer("camera"));
        material->writeDescriptor(1, 1, *m_resourceContext->getImageView("envIrrMap"), m_resourceContext->getSampler("linearClamp"));
        material->writeDescriptor(1, 2, *m_resourceContext->getImageView("filteredMap"), m_resourceContext->getSampler("linearMipmap"));
        material->writeDescriptor(1, 3, *m_resourceContext->getImageView("brdfLut"), m_resourceContext->getSampler("linearClamp"));

        for (int i = 0; i < 1; ++i)
        {
            for (int j = 0; j < 1; ++j)
            {
                constexpr float scale = 1.0f / 1.0f;
                const float offset = 20.0f;
                glm::mat4 translation = glm::translate(glm::vec3(i * offset, 0.0f, j * offset));
                auto node = std::make_unique<RenderNode>(*m_transformBuffer, i * 1 + j);
                node->transformPack->M = translation * glm::rotate(glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(scale));
                node->geometry = m_resourceContext->getGeometry("ocean");
                node->pass(MainPass).material = material;

                struct OceanPC
                {
                    float t;
                    uint32_t N;
                };

                OceanPC pc = { m_time, N };
                node->pass(MainPass).setPushConstants(pc);
                m_renderNodes.emplace_back(std::move(node));
            }
        }

        m_renderer->getDevice()->flushDescriptorUpdates();
    }

    OceanScene::~OceanScene()
    {
    }

    void OceanScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);
    }

    void OceanScene::update(float dt)
    {
        m_cameraController->update(dt);
        const auto& V = m_cameraController->getViewMatrix();
        const auto& P = m_cameraController->getProjectionMatrix();

        m_transformBuffer->update(V, P);

        m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

        if (!paused)
            m_time += dt;
    }

    void OceanScene::render()
    {
        m_renderGraph->clearCommandLists();
        m_renderGraph->buildCommandLists(m_renderNodes);
        m_renderGraph->executeCommandLists();
    }

    std::unique_ptr<VulkanImage> OceanScene::createInitialSpectrum()
    {
        constexpr float g = 9.81;
        struct Wind
        {
            glm::vec2 speed;
            glm::vec2 speedNorm;
            float magnitude;
            float Lw;
        };

        Wind wind;
        wind.speed = { 40, 40 };
        wind.magnitude = std::sqrt(glm::dot(wind.speed, wind.speed));
        wind.speedNorm = wind.speed / wind.magnitude;
        wind.Lw = wind.magnitude * wind.magnitude / g;

        auto calculatePhillipsSpectrum = [](const Wind& wind, const glm::vec2& k)
        {
            const float kLen2 = glm::dot(k, k) + 0.000001;
            const glm::vec2 kNorm = kLen2 == 0.0f ? glm::vec2(0.0f) : k / std::sqrtf(kLen2);

            const float A = 100.0f;
            const float midterm = std::exp(-1.0 / (kLen2 * wind.Lw * wind.Lw)) / std::powf(kLen2, 2);
            const float kDotW = glm::dot(kNorm, wind.speedNorm);

            const float smallWaves = 1.0f;
            const float tail = std::exp(-kLen2 * smallWaves * smallWaves);

            return A * midterm * std::powf(kDotW, 2) * tail;
        };

        std::mt19937 gen{ 0 };
        std::normal_distribution<> distrib(0, 1);
        std::vector<glm::vec4> initialSpectrum;
        for (int i = 0; i < N; ++i)
        {
            for (int j = 0; j < N; ++j)
            {
                const glm::ivec2 idx = glm::ivec2(j, i) - glm::ivec2(N, N) / 2;
                const glm::vec2 k = glm::vec2(idx) * 2.0f * glm::pi<float>() / glm::vec2(Lx, Lx);


                const glm::vec2 h0 = glm::vec2(distrib(gen), distrib(gen)) * 1.0f / std::sqrtf(2.0f) * std::sqrt(calculatePhillipsSpectrum(wind, k));
                glm::vec2 h0Star   = glm::vec2(distrib(gen), distrib(gen)) * 1.0f / std::sqrtf(2.0f) * std::sqrt(calculatePhillipsSpectrum(wind, -k));
                h0Star.y = -h0Star.y;

                initialSpectrum.emplace_back(h0, h0Star);
            }
        }


        auto image = createStorageImage(m_renderer->getDevice(), 1, N, N, VK_FORMAT_R32G32B32A32_SFLOAT);
        std::shared_ptr<VulkanBuffer> stagingBuffer = createStagingBuffer(m_renderer->getDevice(), initialSpectrum.size() * sizeof(glm::vec4), initialSpectrum.data());
        m_renderer->enqueueResourceUpdate([img = image.get(), this, stagingBuffer](VkCommandBuffer cmdBuffer)
        {
            img->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            img->copyFrom(cmdBuffer, *stagingBuffer, 0, 1);

            img->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });

        return image;
    }

    int OceanScene::applyFFT(std::string image)
    {
        int imageLayerRead = 0;
        int imageLayerWrite = 1;
        {
            std::string imageViewRead = image + "View" + std::to_string(imageLayerRead);
            std::string imageViewWrite = image + "View" + std::to_string(imageLayerWrite);

            auto& bitReversePass = m_renderGraph->addComputePass(image + "BitReversePass0");
            bitReversePass.workGroupSize = glm::ivec3(16, 16, 1);
            bitReversePass.numWorkGroups = glm::ivec3(N / 16, N / 16, 1);
            bitReversePass.pipeline = createBitRevPipeline(m_renderer, bitReversePass.workGroupSize);
            bitReversePass.material = std::make_unique<Material>(bitReversePass.pipeline.get());
            bitReversePass.material->writeDescriptor(0, 0, m_resourceContext->getImageView(imageViewRead)->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
            bitReversePass.material->writeDescriptor(0, 1, m_resourceContext->getImageView(imageViewWrite)->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

            bitReversePass.preDispatchCallback = [this, image](VkCommandBuffer cmdBuffer, uint32_t frameIndex)
            {
                auto& pass = m_renderGraph->getNode(image + "BitReversePass0");

                struct PCData
                {
                    uint32_t horizontal;
                    uint32_t numN;
                    float time;
                };

                PCData values = { 1, logN, m_time };
                pass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, values);
            };
            m_renderGraph->addDependency(OscillationPass, image + "BitReversePass0", [this, image, imageLayerRead](const VulkanRenderPass&, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
                {
                    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.image = m_resourceContext->getImage(image)->getHandle();
                    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseArrayLayer = imageLayerRead;
                    barrier.subresourceRange.layerCount = 1;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;

                    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);
                });
            logger->info("{} R: {} W: {}", image + "BitReversePass0", imageLayerRead, imageLayerWrite);

            std::swap(imageLayerRead, imageLayerWrite);
        }

        // Horizontal
        for (int i = 0; i < logN; ++i)
        {
            std::string imageViewRead = image + "View" + std::to_string(imageLayerRead);
            std::string imageViewWrite = image + "View" + std::to_string(imageLayerWrite);


            std::string name = image + "TrueFFTPass" + std::to_string(i);
            auto& fftPass = m_renderGraph->addComputePass(name);
            fftPass.workGroupSize = glm::ivec3(16, 16, 1);
            fftPass.numWorkGroups = glm::ivec3(N / 2 / 16, N / 16, 1);
            fftPass.pipeline = createTrueFFTPipeline(m_renderer, fftPass.workGroupSize, "ifft.comp");
            fftPass.material = std::make_unique<Material>(fftPass.pipeline.get());
            fftPass.material->writeDescriptor(0, 0, m_resourceContext->getImageView(imageViewRead)->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
            fftPass.material->writeDescriptor(0, 1, m_resourceContext->getImageView(imageViewWrite)->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

            fftPass.preDispatchCallback = [this, i, &fftPass](VkCommandBuffer cmdBuffer, uint32_t frameIndex)
            {
                struct PCData
                {
                    int32_t passId;
                    float time;
                    int32_t numN;
                };

                PCData values = { i + 1, m_time, N };
                fftPass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, values);
            };

            logger->info("{} R: {} W: {}", name, imageLayerRead, imageLayerWrite);

            if (i == 0)
            {
                m_renderGraph->addDependency(image + "BitReversePass0", name, [this, image, imageLayerRead](const VulkanRenderPass&, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
                    {
                        VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                        barrier.image = m_resourceContext->getImage(image)->getHandle();
                        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        barrier.subresourceRange.baseArrayLayer = imageLayerRead;
                        barrier.subresourceRange.layerCount = 1;
                        barrier.subresourceRange.baseMipLevel = 0;
                        barrier.subresourceRange.levelCount = 1;

                        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 0, nullptr, 0, nullptr, 1, &barrier);
                    });
            }

            if (i > 0)
            {
                std::string prevName = image + "TrueFFTPass" + std::to_string(i - 1);
                m_renderGraph->addDependency(prevName, name, [this, image, imageLayerRead](const VulkanRenderPass&, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
                    {
                        VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                        barrier.image = m_resourceContext->getImage(image)->getHandle();
                        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        barrier.subresourceRange.baseArrayLayer = imageLayerRead;
                        barrier.subresourceRange.layerCount = 1;
                        barrier.subresourceRange.baseMipLevel = 0;
                        barrier.subresourceRange.levelCount = 1;

                        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 0, nullptr, 0, nullptr, 1, &barrier);
                    });
            }

            std::swap(imageLayerRead, imageLayerWrite);
        }

        {
            std::string imageViewRead = image + "View" + std::to_string(imageLayerRead);
            std::string imageViewWrite = image + "View" + std::to_string(imageLayerWrite);
            auto& bitReversePass2 = m_renderGraph->addComputePass(image + "BitReversePass1");
            bitReversePass2.workGroupSize = glm::ivec3(16, 16, 1);
            bitReversePass2.numWorkGroups = glm::ivec3(N / 16, N / 16, 1);
            bitReversePass2.pipeline = createBitRevPipeline(m_renderer, bitReversePass2.workGroupSize);
            bitReversePass2.material = std::make_unique<Material>(bitReversePass2.pipeline.get());
            bitReversePass2.material->writeDescriptor(0, 0, m_resourceContext->getImageView(imageViewRead)->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
            bitReversePass2.material->writeDescriptor(0, 1, m_resourceContext->getImageView(imageViewWrite)->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

            bitReversePass2.preDispatchCallback = [this, image](VkCommandBuffer cmdBuffer, uint32_t frameIndex)
            {
                auto& pass = m_renderGraph->getNode(image + "BitReversePass1");

                struct PCData
                {
                    uint32_t horizontal;
                    uint32_t numN;
                    float time;
                };

                PCData values = { 0, logN, m_time };
                pass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, values);
            };

            logger->info("{} R: {} W: {}", image + "BitReversePass1", imageLayerRead, imageLayerWrite);

            m_renderGraph->addDependency(image + "TrueFFTPass" + std::to_string(logN - 1), image + "BitReversePass1", [this, image, imageLayerRead](const VulkanRenderPass&, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
                {
                    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.image = m_resourceContext->getImage(image)->getHandle();
                    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseArrayLayer = imageLayerRead;
                    barrier.subresourceRange.layerCount = 1;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;

                    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);
                });
            std::swap(imageLayerRead, imageLayerWrite);
        }


        // Vertical
        std::string finalImageView;
        for (int i = 0; i < logN; ++i)
        {
            std::string imageViewRead = image + "View" + std::to_string(imageLayerRead);
            std::string imageViewWrite = image + "View" + std::to_string(imageLayerWrite);

            std::string name = image + "TrueFFTPassVert" + std::to_string(i);
            auto& fftPass = m_renderGraph->addComputePass(name);
            fftPass.workGroupSize = glm::ivec3(16, 16, 1);
            fftPass.numWorkGroups = glm::ivec3(N / 16, N / 16 / 2, 1);
            fftPass.pipeline = createTrueFFTPipeline(m_renderer, fftPass.workGroupSize, "ifft-vert.comp");
            fftPass.material = std::make_unique<Material>(fftPass.pipeline.get());
            fftPass.material->writeDescriptor(0, 0, m_resourceContext->getImageView(imageViewRead)->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
            fftPass.material->writeDescriptor(0, 1, m_resourceContext->getImageView(imageViewWrite)->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

            logger->info("{} R: {} W: {}", name, imageLayerRead, imageLayerWrite);

            fftPass.preDispatchCallback = [this, i, &fftPass](VkCommandBuffer cmdBuffer, uint32_t frameIndex)
            {
                struct PCData
                {
                    int32_t passId;
                    float time;
                    int32_t numN;
                };

                PCData values = { i + 1, m_time, N };
                fftPass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, values);
            };

            if (i == logN - 1)
            {
                finalImageView = imageViewWrite;
                m_renderGraph->addDependency(name, MainPass, [this, image, imageLayerWrite](const VulkanRenderPass&, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
                    {
                        VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                        barrier.image = m_resourceContext->getImage(image)->getHandle();
                        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        barrier.subresourceRange.baseArrayLayer = imageLayerWrite;
                        barrier.subresourceRange.layerCount = 1;
                        barrier.subresourceRange.baseMipLevel = 0;
                        barrier.subresourceRange.levelCount = 1;

                        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                            0, 0, nullptr, 0, nullptr, 1, &barrier);
                    });
            }
            else if (i == 0)
            {
                m_renderGraph->addDependency(image + "BitReversePass1", name, [this, image, imageLayerRead](const VulkanRenderPass&, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
                    {
                        VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                        barrier.image = m_resourceContext->getImage(image)->getHandle();
                        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        barrier.subresourceRange.baseArrayLayer = imageLayerRead;
                        barrier.subresourceRange.layerCount = 1;
                        barrier.subresourceRange.baseMipLevel = 0;
                        barrier.subresourceRange.levelCount = 1;

                        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 0, nullptr, 0, nullptr, 1, &barrier);
                    });
            }

            if (i > 0)
            {
                std::string prevName = image + "TrueFFTPassVert" + std::to_string(i - 1);
                m_renderGraph->addDependency(prevName, name, [this, image, imageLayerRead](const VulkanRenderPass&, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
                    {
                        VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                        barrier.image = m_resourceContext->getImage(image)->getHandle();
                        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        barrier.subresourceRange.baseArrayLayer = imageLayerRead;
                        barrier.subresourceRange.layerCount = 1;
                        barrier.subresourceRange.baseMipLevel = 0;
                        barrier.subresourceRange.levelCount = 1;

                        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 0, nullptr, 0, nullptr, 1, &barrier);
                    });
            }

            std::swap(imageLayerRead, imageLayerWrite);
        }

        return imageLayerRead;
    }
}