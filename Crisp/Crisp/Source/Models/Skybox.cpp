#include "Skybox.hpp"

#include "IO/ImageFileBuffer.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/Pipelines/SkyboxPipeline.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/Texture.hpp"
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
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
    {
        std::vector<VertexAttributeDescriptor> vertexAttribs = { VertexAttribute::Position };
        m_cubeGeometry = std::make_unique<Geometry>(m_renderer, renderer->getResourcesPath() / "Meshes/cube.obj", vertexAttribs);

        m_transformBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        const std::filesystem::path cubeMapDir = renderer->getResourcesPath() / "Textures/Cubemaps" / cubeMapFolder;

        std::array<std::unique_ptr<ImageFileBuffer>, NumCubeMapFaces> cubeMapImages;
        for (std::size_t i = 0; i < NumCubeMapFaces; ++i)
            cubeMapImages[i] = std::make_unique<ImageFileBuffer>(cubeMapDir / (filenames[i] + ".jpg"));

        uint32_t width  = cubeMapImages[0]->getWidth();
        uint32_t height = cubeMapImages[0]->getHeight();

        m_cubeMap = std::make_unique<VulkanImage>(m_device, VkExtent3D{ width, height, 1u }, static_cast<uint32_t>(NumCubeMapFaces), 1,
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

        for (uint32_t i = 0; i < NumCubeMapFaces; ++i)
            fillLayer(*m_cubeMap, m_renderer, cubeMapImages[i]->getData(), width * height * 4, i);

        m_cubeMapView = m_cubeMap->createView(VK_IMAGE_VIEW_TYPE_CUBE, 0, static_cast<uint32_t>(cubeMapImages.size()));

        // create sampler
        m_sampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_pipeline = createSkyboxPipeline(m_renderer, renderPass, 0);
        m_material = std::make_unique<Material>(m_pipeline.get());
        m_material->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo());
        m_material->writeDescriptor(0, 1, 0, m_cubeMapView->getDescriptorInfo(m_sampler->getHandle()));
        m_device->flushDescriptorUpdates();
    }

    Skybox::Skybox(Renderer* renderer, const VulkanRenderPass& renderPass, std::unique_ptr<VulkanImage> image, std::unique_ptr<VulkanImageView> imageView)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
        , m_cubeMap(std::move(image))
        , m_cubeMapView(std::move(imageView))
    {
        std::vector<VertexAttributeDescriptor> vertexAttribs = { VertexAttribute::Position };
        m_cubeGeometry = std::make_unique<Geometry>(m_renderer, renderer->getResourcesPath() / "Meshes/cube.obj", vertexAttribs);

        m_transformBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        // create sampler
        m_sampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_pipeline = createSkyboxPipeline(m_renderer, renderPass, 0);
        m_material = std::make_unique<Material>(m_pipeline.get());
        m_material->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo());
        m_material->writeDescriptor(0, 1, 0, m_cubeMapView->getDescriptorInfo(m_sampler->getHandle()));
        m_device->flushDescriptorUpdates();
    }

    Skybox::~Skybox()
    {
    }

    void Skybox::updateTransforms(const glm::mat4& V, const glm::mat4& P)
    {
        m_transformPack.MV  = glm::mat4(glm::mat3(V)) * m_transformPack.M;
        m_transformPack.MVP = P * m_transformPack.MV;

        m_transformBuffer->updateStagingBuffer(&m_transformPack, sizeof(m_transformPack));
    }

    DrawCommand Skybox::createDrawCommand() const
    {
        DrawCommand draw;
        draw.pipeline = m_pipeline.get();
        draw.dynamicBufferViews.push_back({ m_transformBuffer.get(), 0 });
        draw.material = m_material.get();
        draw.geometry = m_cubeGeometry.get();
        draw.setGeometryView(m_cubeGeometry->createIndexedGeometryView());
        return draw;
    }

    RenderNode Skybox::createRenderNode()
    {
        RenderNode node(m_transformBuffer.get(), &m_transformPack, 0);
        node.geometry = m_cubeGeometry.get();
        node.material = m_material.get();
        node.pipeline = m_pipeline.get();
        return node;
    }

    VulkanImageView* Skybox::getSkyboxView() const
    {
        return m_cubeMapView.get();
    }
}