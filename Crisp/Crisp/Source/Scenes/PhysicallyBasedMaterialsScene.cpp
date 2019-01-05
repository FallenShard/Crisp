#include "PhysicallyBasedMaterialsScene.hpp"

#include "Core/Application.hpp"
#include "Camera/CameraController.hpp"

#include "vulkan/VulkanImage.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanSampler.hpp"

#include "IO/ImageFileBuffer.hpp"

#include "Geometry/MeshGeometry.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Renderer/Pipelines/PhysicallyBasedPipeline.hpp"
#include "Renderer/Texture.hpp"

namespace crisp
{
    PhysicallyBasedMaterialsScene::PhysicallyBasedMaterialsScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
        , m_app(app)
    {
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_cameraTransformBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        auto vertexFormat = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord };
        m_sphereMesh = std::make_unique<MeshGeometry>(m_renderer, "sphere.obj", vertexFormat);

        m_transforms.resize(2);
        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        auto texFolder = renderer->getResourcesPath() / "Textures/PbrMaterials/RustedIron";
        auto roughness = std::make_unique<ImageFileBuffer>(texFolder / "roughness.png");
        VkExtent3D extent = { roughness->getWidth(), roughness->getHeight(), 1 };

        m_roughnessTex = std::make_unique<Texture>(m_renderer, extent, 1, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_roughnessTex->fill(roughness->getData(), roughness->getByteSize(), 0, 1);
        m_roughnessTexView = m_roughnessTex->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1);

        auto metallic = std::make_unique<ImageFileBuffer>(texFolder / "metallic.png");
        extent = { metallic->getWidth(), metallic->getHeight(), 1 };

        m_metallicTex = std::make_unique<Texture>(m_renderer, extent, 1, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_metallicTex->fill(metallic->getData(), metallic->getByteSize(), 0, 1);
        m_metallicTexView = m_metallicTex->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1);

        auto diffuse = std::make_unique<ImageFileBuffer>(texFolder / "diffuse.png");
        extent = { diffuse->getWidth(), diffuse->getHeight(), 1 };

        m_materialTex = std::make_unique<Texture>(m_renderer, extent, 1, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_materialTex->fill(diffuse->getData(), diffuse->getByteSize(), 0, 1);
        m_materialTexView = m_materialTex->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1);

        m_linearClampSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_mainPass = std::make_unique<SceneRenderPass>(m_renderer);
        m_physBasedPipeline = createPbrPipeline(m_renderer, m_mainPass.get(), 1);
        m_physBasedDesc[0] = { m_physBasedPipeline->allocateDescriptorSet(0) };
        m_physBasedDesc[0].postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        m_physBasedDesc[0].postBufferUpdate(0, 1, m_cameraTransformBuffer->getDescriptorInfo());
        m_physBasedDesc[0].postImageUpdate(0, 2, m_materialTexView->getDescriptorInfo(m_linearClampSampler->getHandle()));
        m_physBasedDesc[0].postImageUpdate(0, 3, m_metallicTexView->getDescriptorInfo(m_linearClampSampler->getHandle()));
        m_physBasedDesc[0].postImageUpdate(0, 4, m_roughnessTexView->getDescriptorInfo(m_linearClampSampler->getHandle()));
        m_physBasedDesc[0].flushUpdates(m_device);

        m_renderer->setSceneImageView(m_mainPass->createRenderTargetView(0, Renderer::NumVirtualFrames));
    }

    PhysicallyBasedMaterialsScene::~PhysicallyBasedMaterialsScene()
    {

    }

    void PhysicallyBasedMaterialsScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_mainPass->recreate();
        m_renderer->setSceneImageView(m_mainPass->createRenderTargetView(0, Renderer::NumVirtualFrames));
    }

    void PhysicallyBasedMaterialsScene::update(float dt)
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
            m_cameraTransformBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));
        }
    }

    void PhysicallyBasedMaterialsScene::render()
    {
        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();
            m_transformsBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
            m_cameraTransformBuffer->updateDeviceBuffer(commandBuffer, frameIdx);
        });

        m_renderer->enqueueDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            auto frameIdx = m_renderer->getCurrentVirtualFrameIndex();

            m_mainPass->begin(commandBuffer);

            m_physBasedPipeline->bind(commandBuffer);
            m_physBasedDesc[0].setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIdx) + 1 * sizeof(TransformPack));
            m_physBasedDesc[0].setDynamicOffset(1, m_cameraTransformBuffer->getDynamicOffset(frameIdx));
            m_physBasedDesc[0].bind(commandBuffer, m_physBasedPipeline->getPipelineLayout()->getHandle());
            m_sphereMesh->bindGeometryBuffers(commandBuffer);
            m_sphereMesh->draw(commandBuffer);

            m_mainPass->end(commandBuffer);

            m_mainPass->getRenderTarget(0)->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIdx, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
    }
}