#include "AmbientOcclusionScene.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "IO/ImageFileBuffer.hpp"
#include "Camera/CameraController.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph.hpp"
#include "Renderer/Material.hpp"

#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/RenderPasses/AmbientOcclusionPass.hpp"
#include "Renderer/Pipelines/UniformColorPipeline.hpp"
#include "Renderer/Pipelines/NormalPipeline.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Renderer/Pipelines/SsaoPipeline.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/VulkanImageUtils.hpp"

#include "Geometry/Geometry.hpp"
#include "Geometry/MeshGenerators.hpp"

#include "vulkan/VulkanSampler.hpp"
#include "vulkan/VulkanImage.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanDevice.hpp"

#include "Models/Skybox.hpp"

#include "GUI/Form.hpp"
#include "AmbientOcclusionPanel.hpp"

#include <CrispCore/Math/Warp.hpp>

#include <random>

namespace crisp
{
    AmbientOcclusionScene::AmbientOcclusionScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
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
            glm::vec3 s = r * warp::squareToUniformHemisphere(glm::vec2(x, y));
            float scale = float(i) / float(512.0f);
            scale = glm::mix(0.1f, 1.0f, scale * scale);
            samples[i].x = s.x * scale;
            samples[i].y = s.y * scale;
            samples[i].z = s.z * scale;
            samples[i].w = 1.0f;
        }

        std::vector<glm::vec4> noiseTexData;
        for (int i = 0; i < 16; i++)
        {
            glm::vec3 s(distribution(randomEngine) * 2.0f - 1.0f, distribution(randomEngine) * 2.0f - 1.0f, 0.0f);
            s = glm::normalize(s);
            noiseTexData.push_back(glm::vec4(s, 1.0f));
        }

        VkImageCreateInfo noiseTexInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        noiseTexInfo.flags         = 0;
        noiseTexInfo.imageType     = VK_IMAGE_TYPE_2D;
        noiseTexInfo.format        = VK_FORMAT_R32G32B32A32_SFLOAT;
        noiseTexInfo.extent        = { 4, 4, 1u };
        noiseTexInfo.mipLevels     = 1;
        noiseTexInfo.arrayLayers   = 1;
        noiseTexInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        noiseTexInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        noiseTexInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        noiseTexInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        noiseTexInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        auto noiseTex = createTexture(m_renderer, noiseTexData.size() * sizeof(glm::vec4), noiseTexData.data(), noiseTexInfo, VK_IMAGE_ASPECT_COLOR_BIT);
        auto noiseTexView = noiseTex->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1);

        m_noiseSampler       = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_REPEAT);
        m_nearestSampler     = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        m_linearClampSampler = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_sampleBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(samples), BufferUpdatePolicy::Constant, samples);

        m_transforms.resize(2);
        m_transforms[0].M = glm::translate(glm::vec3(0.0f, -1.0f, 0.0f)) * glm::scale(glm::vec3(50.0f, 1.0f, 50.0f));
        m_transformBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        auto mainPass = std::make_unique<SceneRenderPass>(m_renderer);
        auto ssaoPass = std::make_unique<AmbientOcclusionPass>(m_renderer);

        auto colorPipeline = createColorPipeline(m_renderer, mainPass.get());
        auto colorMaterial = std::make_unique<Material>(colorPipeline.get());
        colorMaterial->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));

        auto normalPipeline = createNormalPipeline(m_renderer, mainPass.get());

        auto ssaoPipeline   = createSsaoPipeline(m_renderer, ssaoPass.get());
        auto ssaoMaterial = std::make_unique<Material>(ssaoPipeline.get());
        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
        {
            ssaoMaterial->writeDescriptor(0, 0, i, mainPass->getRenderTargetView(0, i).getDescriptorInfo(*m_nearestSampler));
            ssaoMaterial->writeDescriptor(0, 1, i, m_cameraBuffer->getDescriptorInfo());
            ssaoMaterial->writeDescriptor(0, 2, i, m_sampleBuffer->getDescriptorInfo());
            ssaoMaterial->writeDescriptor(0, 3, i, noiseTexView->getDescriptorInfo(*m_noiseSampler));
        }
        ssaoMaterial->addDynamicBufferInfo(*m_cameraBuffer, 0);

        m_renderer->getDevice()->flushDescriptorUpdates();

        std::vector<VertexAttributeDescriptor> shadowFormat = { VertexAttribute::Position };
        auto planeGeometry = std::make_unique<Geometry>(m_renderer, createPlaneMesh(shadowFormat));

        std::vector<VertexAttributeDescriptor> vertexFormat = { VertexAttribute::Position, VertexAttribute::Normal };
        auto sponzaGeometry = std::make_unique<Geometry>(m_renderer, m_renderer->getResourcesPath() / "Meshes/sponza_fixed.obj", vertexFormat);

        m_skybox = std::make_unique<Skybox>(m_renderer, *mainPass, "Creek");

        m_images.push_back(std::move(noiseTex));
        m_imageViews.push_back(std::move(noiseTexView));

        m_geometries.push_back(std::move(planeGeometry));
        m_geometries.push_back(std::move(sponzaGeometry));
        m_pipelines.push_back(std::move(colorPipeline));
        m_pipelines.push_back(std::move(normalPipeline));
        m_pipelines.push_back(std::move(ssaoPipeline));
        m_materials.push_back(std::move(colorMaterial));
        m_materials.push_back(std::move(ssaoMaterial));

        m_renderGraph = std::make_unique<RenderGraph>(m_renderer);
        m_renderGraph->addRenderPass("mainPass", std::move(mainPass));
        m_renderGraph->addRenderPass("ssaoPass", std::move(ssaoPass));
        m_renderGraph->addRenderTargetLayoutTransition("mainPass", "ssaoPass", 0);
        m_renderGraph->addRenderTargetLayoutTransition("ssaoPass", "SCREEN", 0);
        m_renderGraph->sortRenderPasses();

        m_renderer->setSceneImageView(m_renderGraph->getNode("ssaoPass").renderPass.get(), 0);

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

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode("ssaoPass").renderPass.get(), 0);

        auto* mainPass = m_renderGraph->getNode("mainPass").renderPass.get();
        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
            m_materials[1]->writeDescriptor(0, 0, i, mainPass->getRenderTargetView(0, i).getDescriptorInfo(*m_nearestSampler));
    }

    void AmbientOcclusionScene::update(float dt)
    {
        m_cameraController->update(dt);
        m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

        auto& V = m_cameraController->getViewMatrix();
        auto& P = m_cameraController->getProjectionMatrix();

        for (auto& trans : m_transforms)
        {
            trans.MV  = V * trans.M;
            trans.MVP = P * trans.MV;
        }
        m_transformBuffer->updateStagingBuffer(m_transforms.data(), m_transforms.size() * sizeof(TransformPack));

        m_skybox->updateTransforms(V, P);
    }

    void AmbientOcclusionScene::render()
    {
        m_renderGraph->clearCommandLists();

        auto& mainPass = m_renderGraph->getNode("mainPass");
        auto& ssaoPass = m_renderGraph->getNode("ssaoPass");

        DrawCommand dc;
        dc.pipeline = m_materials[0]->getPipeline();
        dc.material = m_materials[0].get();
        dc.dynamicBuffers.push_back({ *m_transformBuffer, 0 * sizeof(TransformPack) });
        for (const auto& info : dc.material->getDynamicBufferInfos())
            dc.dynamicBuffers.push_back(info);
        dc.setPushConstants(glm::vec4(0.2f, 0.0f, 0.0f, 1.0f));
        dc.geometry = m_geometries[0].get();
        dc.setGeometryView(dc.geometry->createIndexedGeometryView());

        mainPass.addCommand(std::move(dc));

        DrawCommand sponza;
        sponza.pipeline = m_pipelines[1].get();
        sponza.material = m_materials[0].get();
        sponza.dynamicBuffers.push_back({ *m_transformBuffer, 1 * sizeof(TransformPack) });
        for (const auto& info : sponza.material->getDynamicBufferInfos())
            sponza.dynamicBuffers.push_back(info);
        sponza.setPushConstants(glm::vec4(0.2f, 0.0f, 0.0f, 1.0f));
        sponza.geometry = m_geometries[1].get();
        sponza.setGeometryView(sponza.geometry->createIndexedGeometryView());

        mainPass.addCommand(std::move(sponza));
        mainPass.addCommand(m_skybox->createDrawCommand());

        DrawCommand ssao;
        ssao.pipeline = m_pipelines[2].get();
        ssao.material = m_materials[1].get();
        for (const auto& info : ssao.material->getDynamicBufferInfos())
            ssao.dynamicBuffers.push_back(info);
        ssao.setPushConstants(m_numSamples, m_radius);
        ssao.geometry = m_renderer->getFullScreenGeometry();
        ssao.setGeometryView(ssao.geometry->createIndexedGeometryView());

        ssaoPass.addCommand(std::move(ssao));

        m_renderGraph->executeDrawCommands();
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