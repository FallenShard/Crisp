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

#include "Renderer/RenderGraph.hpp"

#include "Core/LuaConfig.hpp"

#include "Geometry/MeshGenerators.hpp"

#include "Renderer/VulkanImageUtils.hpp"

namespace crisp
{
    PhysicallyBasedMaterialsScene::PhysicallyBasedMaterialsScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
    {
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        m_renderGraph = std::make_unique<RenderGraph>(m_renderer);

        auto& mainPassNode = m_renderGraph->addRenderPass("mainPass", std::make_unique<SceneRenderPass>(m_renderer));
        m_renderGraph->addDependency("mainPass", "SCREEN", [](const VulkanRenderPass& pass, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
        {
            pass.getRenderTarget(0)->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIndex, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
        m_renderer->setSceneImageView(mainPassNode.renderPass->createRenderTargetView(0, Renderer::NumVirtualFrames));

        VulkanDevice* device = m_renderer->getDevice();
        m_linearClampSampler = std::make_unique<VulkanSampler>(device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        auto vertexFormat = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent };

        m_geometries.emplace_back(createGeometry(m_renderer, createSphereMesh(vertexFormat)));

        m_transforms.resize(2);
        m_transformBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        m_physBasedPipeline = createPbrPipeline(m_renderer, mainPassNode.renderPass.get(), 3);

        LuaConfig config(m_renderer->getResourcesPath() / "Scripts/scene.lua");
        auto texFolder = config.get<std::string>("texture").value_or("RustedIron");
        auto pbrMaterial = createPbrMaterial(texFolder);
        m_materials.push_back(std::move(pbrMaterial));

        m_renderer->setSceneImageView(m_renderGraph->getNode("mainPass").renderPass->createRenderTargetView(0, Renderer::NumVirtualFrames));
    }

    PhysicallyBasedMaterialsScene::~PhysicallyBasedMaterialsScene()
    {
    }

    void PhysicallyBasedMaterialsScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode("mainPass").renderPass->createRenderTargetView(0, Renderer::NumVirtualFrames));
    }

    void PhysicallyBasedMaterialsScene::update(float dt)
    {
        auto viewChanged = m_cameraController->update(dt);
        if (viewChanged)
            m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));

        auto& V = m_cameraController->getViewMatrix();
        auto& P = m_cameraController->getProjectionMatrix();

        for (auto& trans : m_transforms)
        {
            trans.MV  = V * trans.M;
            trans.MVP = P * trans.MV;
        }
        m_transformBuffer->updateStagingBuffer(m_transforms.data(), m_transforms.size() * sizeof(TransformPack));
    }

    void PhysicallyBasedMaterialsScene::render()
    {
        m_renderGraph->clearCommandLists();

        auto& mainPass = m_renderGraph->getNode("mainPass");

        DrawCommand drawCommand;
        drawCommand.pipeline = m_physBasedPipeline.get();
        drawCommand.material = m_materials[0].get();
        drawCommand.dynamicBuffers.push_back({ *m_transformBuffer, 1 * sizeof(TransformPack) });
        for (const auto& info : m_materials[0]->getDynamicBufferInfos())
            drawCommand.dynamicBuffers.push_back(info);
        drawCommand.geometry = m_geometries[0].get();
        drawCommand.setGeometryView(m_geometries[0]->createIndexedGeometryView());


        mainPass.addCommand(std::move(drawCommand));

        m_renderGraph->executeDrawCommands();
    }

    std::unique_ptr<Material> PhysicallyBasedMaterialsScene::createPbrMaterial(const std::string& type)
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
                material->writeDescriptor(0, 2 + i, 0, *m_imageViews.back(), m_linearClampSampler->getHandle());
            }
            else
            {
                logWarning("Texture type {} is using default values for '{}'\n", type, texNames[i]);
                material->writeDescriptor(0, 2 + i, 0, *m_imageViews[i], m_linearClampSampler->getHandle());
            }
        }

        material->writeDescriptor(0, 7, 0, *m_imageViews[0], m_linearClampSampler->getHandle());
        return material;
    }
}