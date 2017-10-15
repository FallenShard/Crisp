#include "BoxDrawer.hpp"

#include "Camera/CameraController.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/IndexBuffer.hpp"
#include "Renderer/UniformBuffer.hpp"

#include "Techniques/CascadedShadowMapper.hpp"

#include "Geometry/TriangleMesh.hpp"
#include "Geometry/MeshGeometry.hpp"

#include "Renderer/Pipelines/OutlinePipeline.hpp"

namespace crisp
{
    BoxDrawer::BoxDrawer(VulkanRenderer* renderer, uint32_t numBoxes, VulkanRenderPass* renderPass)
        : m_renderer(renderer)
        , m_numBoxes(numBoxes)
    {
        std::vector<glm::u16vec2> lines =
        {
            { 0, 1 },
            { 1, 2 },
            { 2, 3 },
            { 3, 0 },

            { 4, 5 },
            { 5, 6 },
            { 6, 7 },
            { 7, 4 },

            { 0, 4 },
            { 3, 7 },
            { 2, 6 },
            { 1, 5 }
        };

        m_indexBuffer = std::make_unique<IndexBuffer>(m_renderer, lines);
        m_numIndices  = static_cast<uint32_t>(lines.size()) * 2;

        std::vector<glm::vec3> cubePoints =
        {
            { -0.5f, -0.5f, +0.5f },
            { +0.5f, -0.5f, +0.5f },
            { +0.5f, +0.5f, +0.5f },
            { -0.5f, +0.5f, +0.5f },

            { -0.5f, -0.5f, -0.5f },
            { +0.5f, -0.5f, -0.5f },
            { +0.5f, +0.5f, -0.5f },
            { -0.5f, +0.5f, -0.5f }
        };

        m_cubeVertexBuffer = std::make_unique<VertexBuffer>(m_renderer, cubePoints);
        m_cubeVertexBindingGroup =
        {
            { m_cubeVertexBuffer->get(), 0 }
        };

        m_transforms.resize(m_numBoxes);

        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(Transforms), BufferUpdatePolicy::PerFrame);

        m_outlinePipeline = std::make_unique<OutlinePipeline>(m_renderer, renderPass);
        m_outlineDesc =
        {
            m_outlinePipeline->allocateDescriptorSet(0)
        };
        m_outlineDesc.postBufferUpdate(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_transformsBuffer->getDescriptorInfo(0, sizeof(Transforms)));
        m_outlineDesc.flushUpdates(m_renderer->getDevice());
    }

    BoxDrawer::~BoxDrawer()
    {
    }

    void BoxDrawer::setBoxTransforms(const std::vector<glm::vec3>& centers, const std::vector<glm::vec3>& scales)
    {
        for (size_t i = 0; i < m_numBoxes; i++)
        {
            glm::mat4 scale = glm::scale(scales[i]);
            glm::mat4 translation = glm::translate(centers[i]);
            m_transforms[i].M = translation * scale;
        }
    }

    void BoxDrawer::update(const glm::mat4& V, const glm::mat4& P)
    {
        for (auto& trans : m_transforms)
        {
            trans.MV  = V * trans.M;
            trans.MVP = P * trans.MV;
        }

        m_transformsBuffer->updateStagingBuffer(m_transforms.data(), m_transforms.size() * sizeof(Transforms));
    }

    void BoxDrawer::updateDeviceBuffers(VkCommandBuffer commandBuffer, uint32_t frameIndex)
    {
        m_transformsBuffer->updateDeviceBuffer(commandBuffer, frameIndex);
    }

    void BoxDrawer::render(VkCommandBuffer commandBuffer, uint32_t frameIndex)
    {
        m_outlinePipeline->bind(commandBuffer);

        m_indexBuffer->bind(commandBuffer, 0);
        m_cubeVertexBindingGroup.bind(commandBuffer);

        for (int i = 0; i < m_numBoxes; i++)
        {
            m_outlineDesc.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(frameIndex) + i * sizeof(Transforms));
            m_outlineDesc.bind(commandBuffer, m_outlinePipeline->getPipelineLayout());
            m_outlinePipeline->setPushConstant(commandBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, 0, glm::vec4(1.0f, 0.0f, 0.5f, 1.0f));
            vkCmdDrawIndexed(commandBuffer, m_numIndices, 1, 0, 0, 0);
        }
    }
}