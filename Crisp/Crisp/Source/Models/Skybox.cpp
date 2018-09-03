#include "Skybox.hpp"

#include "IO/ImageFileBuffer.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/Pipelines/SkyboxPipeline.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/Texture.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanSampler.hpp"
#include "vulkan/VulkanImageView.hpp"
#include "Geometry/Geometry.hpp"
#include "Geometry/TriangleMesh.hpp"
#include "Renderer/Material.hpp"

namespace crisp
{
    Skybox::Skybox(Renderer* renderer, const VulkanRenderPass& renderPass, const std::string& cubeMapFolder)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
    {
        m_cubeGeometry = createGeometry(m_renderer, renderer->getResourcesPath() / "Meshes/cube.obj", { VertexAttribute::Position });

        m_transformBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        std::vector<std::string> fileNames =
        {
            "left",
            "right",
            "top",
            "bottom",
            "back",
            "front"
        };
        std::vector<std::unique_ptr<ImageFileBuffer>> cubeMapImages;
        cubeMapImages.reserve(fileNames.size());

        for (auto& fileName : fileNames)
        {
            cubeMapImages.push_back(std::make_unique<ImageFileBuffer>(renderer->getResourcesPath() / "Textures/Cubemaps" / cubeMapFolder / (fileName + ".jpg")));
        }

        auto width  = cubeMapImages[0]->getWidth();
        auto height = cubeMapImages[0]->getHeight();

        m_cubeMap = std::make_unique<Texture>(m_renderer, VkExtent3D{ width, height, 1u }, static_cast<uint32_t>(cubeMapImages.size()),
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
        for (int i = 0; i < cubeMapImages.size(); i++)
            m_cubeMap->fill(cubeMapImages[i]->getData(), width * height * 4, i, 1);

        m_cubeMapView = m_cubeMap->createView(VK_IMAGE_VIEW_TYPE_CUBE, 0, static_cast<uint32_t>(cubeMapImages.size()));

        // create sampler
        m_sampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_pipeline = createSkyboxPipeline(m_renderer, renderPass, 0);
        m_material = std::make_unique<Material>(m_pipeline.get(), std::vector<uint32_t>{ 0 });
        m_device->postDescriptorWrite(m_material->makeDescriptorWrite(0, 0), m_transformBuffer->getDescriptorInfo());
        m_device->postDescriptorWrite(m_material->makeDescriptorWrite(0, 1), m_cubeMapView->getDescriptorInfo(m_sampler->getHandle()));
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
        draw.dynamicBuffers.push_back({ *m_transformBuffer, 0 });
        draw.material = m_material.get();
        draw.geometry = m_cubeGeometry.get();
        draw.setGeometryView(m_cubeGeometry->createIndexedGeometryView());
        return draw;
    }

    VulkanImageView* Skybox::getSkyboxView() const
    {
        return m_cubeMapView.get();
    }
}