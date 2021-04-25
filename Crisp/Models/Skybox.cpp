#include "Skybox.hpp"

#include "IO/ImageFileBuffer.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/Texture.hpp"
#include "Vulkan/VulkanPipeline.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanSampler.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanImageView.hpp"
#include "Geometry/Geometry.hpp"
#include "Geometry/TriangleMesh.hpp"
#include "Renderer/Material.hpp"
#include "Renderer/VulkanImageUtils.hpp"

namespace crisp
{
    static constexpr std::size_t NumCubeMapFaces = 6;
    static const std::array<const std::string, NumCubeMapFaces> filenames = { "left", "right", "top", "bottom", "back", "front" };

    Skybox::Skybox(Renderer* renderer, const VulkanRenderPass& renderPass, const std::string& cubeMapFolder)
    {
        std::vector<VertexAttributeDescriptor> vertexAttribs = { VertexAttribute::Position };
        m_cubeGeometry = std::make_unique<Geometry>(renderer, renderer->getResourcesPath() / "Meshes/cube.obj", vertexAttribs);

        m_transformBuffer = std::make_unique<UniformBuffer>(renderer, sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        const std::filesystem::path cubeMapDir = renderer->getResourcesPath() / "Textures/Cubemaps" / cubeMapFolder;

        std::array<std::unique_ptr<ImageFileBuffer>, NumCubeMapFaces> cubeMapImages;
        for (std::size_t i = 0; i < NumCubeMapFaces; ++i)
            cubeMapImages[i] = std::make_unique<ImageFileBuffer>(cubeMapDir / (filenames[i] + ".jpg"));

        uint32_t width  = cubeMapImages[0]->getWidth();
        uint32_t height = cubeMapImages[0]->getHeight();

        m_cubeMap = std::make_unique<VulkanImage>(renderer->getDevice(), VkExtent3D{ width, height, 1u }, static_cast<uint32_t>(NumCubeMapFaces), 1,
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

        for (uint32_t i = 0; i < NumCubeMapFaces; ++i)
            fillLayer(*m_cubeMap, renderer, cubeMapImages[i]->getData(), width * height * 4, i);

        m_cubeMapView = m_cubeMap->createView(VK_IMAGE_VIEW_TYPE_CUBE, 0, static_cast<uint32_t>(cubeMapImages.size()));

        // create sampler
        m_sampler = std::make_unique<VulkanSampler>(renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_pipeline = renderer->createPipelineFromLua("Skybox.lua", renderPass, 0);
        m_material = std::make_unique<Material>(m_pipeline.get());
        m_material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        m_material->writeDescriptor(0, 1, m_cubeMapView->getDescriptorInfo(m_sampler.get()));
        renderer->getDevice()->flushDescriptorUpdates();

        m_renderNode.transformBuffer = m_transformBuffer.get();
        m_renderNode.transformPack = &m_transformPack;
        m_renderNode.transformIndex = 0;
        m_renderNode.geometry = m_cubeGeometry.get();
        m_renderNode.pass("mainPass").material = m_material.get();
    }

    Skybox::Skybox(Renderer* renderer, const VulkanRenderPass& renderPass, const VulkanImageView& cubeMapView, const VulkanSampler& sampler)
    {
        std::vector<VertexAttributeDescriptor> vertexAttribs = { VertexAttribute::Position };
        m_cubeGeometry = std::make_unique<Geometry>(renderer, renderer->getResourcesPath() / "Meshes/cube.obj", vertexAttribs);

        m_pipeline = renderer->createPipelineFromLua("Skybox.lua", renderPass, 0);
        m_material = std::make_unique<Material>(m_pipeline.get());

        m_transformBuffer = std::make_unique<UniformBuffer>(renderer, sizeof(TransformPack), BufferUpdatePolicy::PerFrame);
        m_material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        m_material->writeDescriptor(0, 1, cubeMapView.getDescriptorInfo(&sampler));

        renderer->getDevice()->flushDescriptorUpdates();

        m_renderNode.transformBuffer = m_transformBuffer.get();
        m_renderNode.transformPack = &m_transformPack;
        m_renderNode.transformIndex = 0;
        m_renderNode.geometry = m_cubeGeometry.get();
        m_renderNode.pass("mainPass").material = m_material.get();
    }

    Skybox::~Skybox()
    {
    }

    void Skybox::updateTransforms(const glm::mat4& V, const glm::mat4& P)
    {
        m_transformPack.M = glm::mat4(1.0f);
        m_transformPack.MV  = glm::mat4(glm::mat3(V));
        m_transformPack.MVP = P * m_transformPack.MV;

        m_transformBuffer->updateStagingBuffer(m_transformPack);
    }

    const RenderNode& Skybox::getRenderNode()
    {
        return m_renderNode;
    }

    VulkanImageView* Skybox::getSkyboxView() const
    {
        return m_cubeMapView.get();
    }
}