#include "Skybox.hpp"

#include "vulkan/VulkanRenderer.hpp"
#include "vulkan/Pipelines/SkyboxPipeline.hpp"

#include "Vesper/Math/Transform.hpp"
#include "Vesper/Shapes/MeshLoader.hpp"

#include "vulkan/IndexBuffer.hpp"

#include "IO/Image.hpp"

namespace crisp
{
    Skybox::Skybox(VulkanRenderer* renderer, VulkanRenderPass* renderPass)
        : m_renderer(renderer)
        , m_device(&renderer->getDevice())
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        std::vector<glm::uvec3> tempFaces;

        vesper::MeshLoader loader;
        loader.load("cube.obj", positions, normals, texCoords, tempFaces);

        std::vector<glm::u16vec3> faces;
        for (auto& f : tempFaces)
            faces.emplace_back(f.x, f.y, f.z);

        m_buffer      = std::make_unique<VertexBuffer>(m_device, positions);
        m_indexBuffer = std::make_unique<IndexBuffer>(m_device, faces);

        m_vertexBufferGroup =
        {
            { m_buffer->get(), 0 }
        };

        m_pipeline = std::make_unique<SkyboxPipeline>(m_renderer, renderPass);
        m_descriptorSetGroup = 
        {
            m_pipeline->allocateDescriptorSet(0)
        };

        m_transformsBuffer = std::make_unique<UniformBuffer>(m_device, sizeof(Transforms), BufferUpdatePolicy::PerFrame);

        std::vector<std::string> fileNames =
        {
            "left",
            "right",
            "top",
            "bottom",
            "back",
            "front"
        };
        std::vector<std::unique_ptr<Image>> cubeMapImages;
        cubeMapImages.reserve(fileNames.size());

        for (auto& fileName : fileNames)
        {
            cubeMapImages.push_back(std::make_unique<Image>("Resources/Textures/Cubemaps/Creek/" + fileName + ".jpg"));
        }

        auto width = cubeMapImages[0]->getWidth();
        auto height = cubeMapImages[0]->getHeight();

        m_texture = m_device->createDeviceImageArray(width, height, static_cast<uint32_t>(cubeMapImages.size()), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

        for (int i = 0; i < cubeMapImages.size(); i++)
        {
            auto byteSize = width * height * 4;
            m_device->fillDeviceImage(m_texture, cubeMapImages[i]->getData(), byteSize, { width, height, 1u }, 1, i);
        }
            
        m_device->transitionImageLayout(m_texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 6);
        
        // create view
        m_imageView = m_device->createImageView(m_texture, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, 6);
        
        // create sampler
        m_sampler = m_device->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_descriptorSetGroup.postBufferUpdate(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, { m_transformsBuffer->get(), 0, sizeof(Transforms) });
        m_descriptorSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { m_sampler, m_imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        m_descriptorSetGroup.flushUpdates(m_device);
    }

    Skybox::~Skybox()
    {
        vkDestroyImageView(m_device->getHandle(), m_imageView, nullptr);
        vkDestroySampler(m_device->getHandle(), m_sampler, nullptr);
    }

    void Skybox::updateTransforms(const glm::mat4& P, const glm::mat4& V)
    {
        m_transforms.MV  = glm::mat4(glm::mat3(V)) * m_transforms.M;
        m_transforms.MVP = P * m_transforms.MV;

        m_transformsBuffer->updateStagingBuffer(&m_transforms, sizeof(Transforms));
    }

    void Skybox::resize(int width, int height)
    {
        m_pipeline->resize(width, height);
    }

    void Skybox::updateDeviceBuffers(VkCommandBuffer& cmdBuffer, uint32_t currentFrameIndex)
    {
        m_transformsBuffer->updateDeviceBuffer(cmdBuffer, currentFrameIndex);
    }

    void Skybox::draw(VkCommandBuffer& cmdBuffer, uint32_t currentFrameIndex) const
    {
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getHandle());
        m_descriptorSetGroup.setDynamicOffset(0, currentFrameIndex * sizeof(Transforms));
        m_descriptorSetGroup.bind(cmdBuffer, m_pipeline->getPipelineLayout());

        m_vertexBufferGroup.bind(cmdBuffer);
        m_indexBuffer->bind(cmdBuffer, 0);
        vkCmdDrawIndexed(cmdBuffer, 36, 1, 0, 0, 0);
    }
}