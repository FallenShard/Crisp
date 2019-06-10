#include "BoxVisualizer.hpp"

#include "Camera/CameraController.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/IndexBuffer.hpp"
#include "Renderer/UniformBuffer.hpp"

#include "Techniques/CascadedShadowMapper.hpp"

#include "Geometry/TriangleMesh.hpp"

#include "Renderer/Pipelines/OutlinePipeline.hpp"

namespace crisp
{
    BoxVisualizer::BoxVisualizer(Renderer* renderer, uint32_t numBoxes, uint32_t numFrusta, const VulkanRenderPass& renderPass)
        : m_renderer(renderer)
        , m_numBoxes(numBoxes)
        , m_frusta(numFrusta)
        , m_stagingBuffers(numFrusta)
    {
        std::vector<glm::uvec2> lines =
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

        for (auto& stagingBuffer : m_stagingBuffers)
            stagingBuffer = createStagingBuffer(renderer->getDevice(), 8 * sizeof(glm::vec3), nullptr);

        for (auto& frustum : m_frusta)
            frustum = std::make_unique<Geometry>(renderer, 8, lines);

        m_outlineTransforms.resize(m_frusta.size() + m_numBoxes);
        m_outlineTransforms[0].M = glm::scale(glm::vec3{ 3.0f });
        m_outlineTransforms[1].M = glm::translate(glm::vec3{ -5.0f, 0.0f, 0.0f }) * glm::scale(glm::vec3{ 3.0f });
        m_outlineTransforms[2].M = glm::translate(glm::vec3{ -15.0f, 0.0f, 0.0f }) * glm::scale(glm::vec3{ 3.0f });
        m_outlineTransforms[3].M = glm::translate(glm::vec3{ -25.0f, 0.0f, 0.0f }) * glm::scale(glm::vec3{ 3.0f });

        m_outlineTransformsBuffer = std::make_unique<UniformBuffer>(m_renderer, m_outlineTransforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        m_outlinePipeline = createOutlinePipeline(m_renderer, &renderPass);
        m_outlineMaterial = std::make_unique<Material>(m_outlinePipeline.get());
        m_outlineMaterial->writeDescriptor(0, 0, 0, m_outlineTransformsBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        /*m_cubeGeometry = std::make_unique<MeshGeometry>(m_renderer, "cube.obj", std::initializer_list<VertexAttribute>{ VertexAttribute::Position });

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
        m_numIndices = static_cast<uint32_t>(lines.size()) * 2;

        m_frustumPoints =
        {
            { -1.0f, -1.0f, -1.0f },
            { +1.0f, -1.0f, -1.0f },
            { +1.0f, +1.0f, -1.0f },
            { -1.0f, +1.0f, -1.0f },

            { -2.0f, -2.0f, -5.0f },
            { +2.0f, -2.0f, -5.0f },
            { +2.0f, +2.0f, -5.0f },
            { -2.0f, +2.0f, -5.0f }
        };

        m_frusta.resize(4);
        for (auto& frustum : m_frusta)
        {
            frustum.vertexBuffer = std::make_unique<VertexBuffer>(m_renderer, AbstractCamera::NumFrustumPoints * sizeof(glm::vec3), BufferUpdatePolicy::PerFrame);
            frustum.vertexBindingGroup =
            {
                { frustum.vertexBuffer->get(), 0 }
            };
        }

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

        m_outlineTransforms.resize(m_numFrusta + m_numBoxes);
        m_outlineTransforms[0].M = glm::scale(glm::vec3{ 3.0f });
        m_outlineTransforms[1].M = glm::translate(glm::vec3{ -5.0f, 0.0f, 0.0f }) * glm::scale(glm::vec3{ 3.0f });
        m_outlineTransforms[2].M = glm::translate(glm::vec3{ -15.0f, 0.0f, 0.0f }) * glm::scale(glm::vec3{ 3.0f });
        m_outlineTransforms[3].M = glm::translate(glm::vec3{ -25.0f, 0.0f, 0.0f }) * glm::scale(glm::vec3{ 3.0f });

        m_outlineTransformsBuffer = std::make_unique<UniformBuffer>(m_renderer, m_outlineTransforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        m_outlinePipeline = createOutlinePipeline(m_renderer, renderPass);
        m_outlineDesc =
        {
            m_outlinePipeline->allocateDescriptorSet(0)
        };
        m_outlineDesc.postBufferUpdate(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_outlineTransformsBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        m_outlineDesc.flushUpdates(m_renderer->getDevice());*/
    }

    BoxVisualizer::~BoxVisualizer()
    {
    }

    void BoxVisualizer::updateFrusta(CascadedShadowMapper* shadowMapper, CameraController* cameraController)
    {
        auto& camera = cameraController->getCamera();

        auto camPos = camera.getPosition();

        m_outlineTransforms[0].M = glm::mat4(1.0f);
        m_outlineTransforms[1].M = glm::mat4(1.0f);
        m_outlineTransforms[2].M = glm::mat4(1.0f);
        m_outlineTransforms[3].M = glm::mat4(1.0f);

        auto& bounds = shadowMapper->getBoundingBoxExtents();
        std::vector<glm::vec3> vertices;
        for (int i = 0; i < 4; i++)
        {
            vertices.clear();
            vertices.emplace_back(bounds[i][0].x, bounds[i][0].y, bounds[i][0].z);
            vertices.emplace_back(bounds[i][1].x, bounds[i][0].y, bounds[i][0].z);
            vertices.emplace_back(bounds[i][1].x, bounds[i][1].y, bounds[i][0].z);
            vertices.emplace_back(bounds[i][0].x, bounds[i][1].y, bounds[i][0].z);
            vertices.emplace_back(bounds[i][0].x, bounds[i][0].y, bounds[i][1].z);
            vertices.emplace_back(bounds[i][1].x, bounds[i][0].y, bounds[i][1].z);
            vertices.emplace_back(bounds[i][1].x, bounds[i][1].y, bounds[i][1].z);
            vertices.emplace_back(bounds[i][0].x, bounds[i][1].y, bounds[i][1].z);
            /*vertices.emplace_back(-5 * i, -5 * i, -5 * i);
            vertices.emplace_back(+5, -5, -5);
            vertices.emplace_back(+5, +5, -5);
            vertices.emplace_back(-5, +5, -5);
            vertices.emplace_back(-5, -5, +5);
            vertices.emplace_back(+5, -5, +5);
            vertices.emplace_back(+5, +5, +5);
            vertices.emplace_back(-5, +5, +5);*/

            m_stagingBuffers[i]->updateFromHost(vertices);

            m_outlineTransforms[i].M = glm::mat4(1.0f);
        }

        for (auto& trans : m_outlineTransforms)
        {
            trans.MV = camera.getViewMatrix() * trans.M;
            trans.MVP = camera.getProjectionMatrix() * trans.MV;
        }

        m_outlineTransformsBuffer->updateStagingBuffer(m_outlineTransforms.data(), m_outlineTransforms.size() * sizeof(TransformPack));

    }

    void BoxVisualizer::updateDeviceBuffers(VkCommandBuffer commandBuffer, uint32_t frameIndex)
    {
        for (int i = 0; i < 4; i++)
        {
            uint32_t offset = frameIndex * 8 * sizeof(glm::vec3);
            m_frusta[i]->getVertexBuffer()->copyFrom(commandBuffer, *m_stagingBuffers[i], 0, offset, 8 * sizeof(glm::vec3));
            m_frusta[i]->setVertexBufferOffset(0, frameIndex * 8 * sizeof(glm::vec3));
        }
    }

    std::vector<DrawCommand> BoxVisualizer::createDrawCommands() const
    {
        static const std::vector<glm::vec4> colors =
        {
            { 1.0f, 0.0f, 0.0f, 1.0f },
            { 1.0f, 1.0f, 0.0f, 1.0f },
            { 0.0f, 1.0f, 0.0f, 1.0f },
            { 0.0f, 0.0f, 1.0f, 1.0f }
        };

        std::vector<DrawCommand> drawCommands(4);
        for (int i = 0; i < 4; ++i)
        {
            drawCommands[i].pipeline = m_outlinePipeline.get();
            drawCommands[i].material = m_outlineMaterial.get();
            drawCommands[i].dynamicBuffers.push_back({ *m_outlineTransformsBuffer, i * sizeof(TransformPack) });
            for (const auto& info : drawCommands[i].material->getDynamicBufferInfos())
                drawCommands[i].dynamicBuffers.push_back(info);
            drawCommands[i].setPushConstants(colors[i]);
            drawCommands[i].geometry = m_frusta[i].get();
            drawCommands[i].setGeometryView(drawCommands[i].geometry->createIndexedGeometryView());
        }

        return drawCommands;
    }
}