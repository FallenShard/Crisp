#include "Skybox.hpp"

#include "IO/ImageFileBuffer.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/Pipelines/SkyboxPipeline.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/Texture.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanSampler.hpp"
#include "vulkan/VulkanImageView.hpp"
#include "Geometry/MeshGeometry.hpp"
#include "Geometry/TriangleMesh.hpp"

namespace crisp
{
    Skybox::Skybox(Renderer* renderer, VulkanRenderPass* renderPass)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
    {
        m_cubeGeometry = std::make_unique<MeshGeometry>(m_renderer, "cube.obj", std::initializer_list<VertexAttribute>{ VertexAttribute::Position });

        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(Transforms), BufferUpdatePolicy::PerFrame);

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
            cubeMapImages.push_back(std::make_unique<ImageFileBuffer>("Resources/Textures/Cubemaps/Creek/" + fileName + ".jpg"));
        }

        auto width  = cubeMapImages[0]->getWidth();
        auto height = cubeMapImages[0]->getHeight();

        m_cubeMap = std::make_unique<Texture>(m_renderer, VkExtent3D{ width, height, 1u }, static_cast<uint32_t>(cubeMapImages.size()),
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
        for (int i = 0; i < cubeMapImages.size(); i++)
        {
            m_cubeMap->fill(cubeMapImages[i]->getData(), width * height * 4, i, 1);
        }
        m_cubeMapView = m_cubeMap->createView(VK_IMAGE_VIEW_TYPE_CUBE, 0, static_cast<uint32_t>(cubeMapImages.size()));

        // create sampler
        m_sampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_pipeline           = std::make_unique<SkyboxPipeline>(m_renderer, renderPass);
        m_descriptorSetGroup = { m_pipeline->allocateDescriptorSet(0) };
        m_descriptorSetGroup.postBufferUpdate(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_transformsBuffer->getDescriptorInfo());
        m_descriptorSetGroup.postImageUpdate(0,  1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_cubeMapView->getDescriptorInfo(m_sampler->getHandle()));
        m_descriptorSetGroup.flushUpdates(m_device);
    }

    Skybox::~Skybox()
    {
    }

    void Skybox::updateTransforms(const glm::mat4& P, const glm::mat4& V)
    {
        m_transforms.MV  = glm::mat4(glm::mat3(V)) * m_transforms.M;
        m_transforms.MVP = P * m_transforms.MV;

        m_transformsBuffer->updateStagingBuffer(&m_transforms, sizeof(m_transforms));
    }

    void Skybox::updateDeviceBuffers(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex)
    {
        m_transformsBuffer->updateDeviceBuffer(cmdBuffer, currentFrameIndex);
    }

    void Skybox::draw(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex) const
    {
        m_descriptorSetGroup.setDynamicOffset(0, currentFrameIndex * sizeof(Transforms));

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getHandle());
        m_descriptorSetGroup.bind(cmdBuffer, m_pipeline->getPipelineLayout());

        m_cubeGeometry->bindGeometryBuffers(cmdBuffer);
        m_cubeGeometry->draw(cmdBuffer);
    }

    VulkanImageView* Skybox::getSkyboxView() const
    {
        return m_cubeMapView.get();
    }
}