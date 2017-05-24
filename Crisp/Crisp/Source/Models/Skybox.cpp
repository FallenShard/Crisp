#include "Skybox.hpp"

#include <Vesper/Shapes/MeshLoader.hpp>

#include "IO/ImageFileBuffer.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/Pipelines/SkyboxPipeline.hpp"
#include "Renderer/IndexBuffer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/Texture.hpp"
#include "Renderer/TextureView.hpp"
#include "vulkan/VulkanDevice.hpp"

namespace crisp
{
    Skybox::Skybox(VulkanRenderer* renderer, VulkanRenderPass* renderPass)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
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

        m_buffer      = std::make_unique<VertexBuffer>(m_renderer, positions);
        m_indexBuffer = std::make_unique<IndexBuffer>(m_renderer, faces);

        m_vertexBufferGroup =
        {
            { m_buffer->get(), 0 }
        };

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

        m_texture = std::make_unique<Texture>(m_renderer, VkExtent3D{ width, height, 1u }, static_cast<uint32_t>(cubeMapImages.size()),
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
        for (int i = 0; i < cubeMapImages.size(); i++)
        {
            m_texture->fill(cubeMapImages[i]->getData(), width * height * 4, i, 1);
        }
        m_textureView = m_texture->createView(VK_IMAGE_VIEW_TYPE_CUBE, 0, static_cast<uint32_t>(cubeMapImages.size()));
        
        // create sampler
        m_sampler = m_device->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_pipeline           = std::make_unique<SkyboxPipeline>(m_renderer, renderPass);
        m_nextPipeline       = std::make_unique<SkyboxPipeline>(m_renderer, renderPass, 1);
        m_descriptorSetGroup = { m_pipeline->allocateDescriptorSet(0) };
        m_descriptorSetGroup.postBufferUpdate(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_transformsBuffer->getDescriptorInfo());
        m_descriptorSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_textureView->getDescriptorInfo(m_sampler));
        m_descriptorSetGroup.flushUpdates(m_device);
    }

    Skybox::~Skybox()
    {
        vkDestroySampler(m_device->getHandle(), m_sampler, nullptr);
    }

    void Skybox::updateTransforms(const glm::mat4& P, const glm::mat4& V)
    {
        m_transforms.MV  = glm::mat4(glm::mat3(V)) * m_transforms.M;
        m_transforms.MVP = P * m_transforms.MV;

        m_transformsBuffer->updateStagingBuffer(&m_transforms, sizeof(Transforms));
    }

    void Skybox::updateDeviceBuffers(VkCommandBuffer& cmdBuffer, uint32_t currentFrameIndex)
    {
        m_transformsBuffer->updateDeviceBuffer(cmdBuffer, currentFrameIndex);
    }

    void Skybox::draw(VkCommandBuffer& cmdBuffer, uint32_t currentFrameIndex, uint32_t subpassIndex) const
    {
        m_descriptorSetGroup.setDynamicOffset(0, currentFrameIndex * sizeof(Transforms));

        if (subpassIndex == 1)
        {
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_nextPipeline->getHandle());
            m_descriptorSetGroup.bind(cmdBuffer, m_nextPipeline->getPipelineLayout());
        }
        else
        {
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getHandle());
            m_descriptorSetGroup.bind(cmdBuffer, m_pipeline->getPipelineLayout());
        }

        m_vertexBufferGroup.bind(cmdBuffer);
        m_indexBuffer->bind(cmdBuffer, 0);
        vkCmdDrawIndexed(cmdBuffer, 36, 1, 0, 0, 0);
    }

    VkImageView Skybox::getSkyboxView() const
    {
        return m_textureView->getHandle();
    }
}