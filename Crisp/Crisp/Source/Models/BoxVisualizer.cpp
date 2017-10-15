#include "BoxVisualizer.hpp"

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
    BoxVisualizer::BoxVisualizer(VulkanRenderer* renderer, uint32_t numBoxes, uint32_t numFrusta, VulkanRenderPass* renderPass)
        : m_renderer(renderer)
        , m_numBoxes(numBoxes)
        , m_numFrusta(numFrusta)
    {
        m_cubeGeometry = std::make_unique<MeshGeometry>(m_renderer, TriangleMesh("cube.obj", { VertexAttribute::Position }));

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

        m_outlineTransformsBuffer = std::make_unique<UniformBuffer>(m_renderer, m_outlineTransforms.size() * sizeof(Transforms), BufferUpdatePolicy::PerFrame);

        m_outlinePipeline = std::make_unique<OutlinePipeline>(m_renderer, renderPass);
        m_outlineDesc =
        {
            m_outlinePipeline->allocateDescriptorSet(0)
        };
        m_outlineDesc.postBufferUpdate(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_outlineTransformsBuffer->getDescriptorInfo(0, sizeof(Transforms)));
        m_outlineDesc.flushUpdates(m_renderer->getDevice());
    }

    BoxVisualizer::~BoxVisualizer()
    {
    }

    void BoxVisualizer::update(const glm::mat4& V, const glm::mat4& P)
    {
        for (auto& trans : m_outlineTransforms)
        {
            trans.MV = V * trans.M;
            trans.MVP = P * trans.MV;
        }

        m_outlineTransformsBuffer->updateStagingBuffer(m_outlineTransforms.data(), m_outlineTransforms.size() * sizeof(Transforms));
    }

    void BoxVisualizer::updateFrusta(CascadedShadowMapper* shadowMapper, CameraController* cameraController)
    {
        auto& camera = cameraController->getCamera();

        auto camPos = camera.getPosition();

        m_outlineTransforms[0].M = glm::mat4(1.0f);
        m_outlineTransforms[1].M = glm::mat4(1.0f);
        m_outlineTransforms[2].M = glm::mat4(1.0f);
        m_outlineTransforms[3].M = glm::mat4(1.0f);

        std::vector<float> splitsNear = { 0.1f, 5.0f, 10.0f, 20.0f };
        std::vector<float> splitsFar = { 5.0f, 10.0f, 20.0f, 50.0f };

        auto lightView = shadowMapper->getLight()->getViewMatrix();

        for (int i = 0; i < 4; i++)
        {
            auto worldPts = camera.getFrustumPoints(splitsNear[i], splitsFar[i]);
            m_frusta[i].vertexBuffer->updateStagingBuffer(worldPts);

            glm::vec3 minCorner(1000.0f);
            glm::vec3 maxCorner(-1000.0f);
            for (auto& pt : worldPts)
            {
                auto lightViewPt = lightView * glm::vec4(pt, 1.0f);

                minCorner = glm::min(minCorner, glm::vec3(lightViewPt));
                maxCorner = glm::max(maxCorner, glm::vec3(lightViewPt));
            }

            glm::vec3 lengths(maxCorner - minCorner);
            auto cubeCenter = (minCorner + maxCorner) / 2.0f;
            auto squareSize = std::max(lengths.x, lengths.y);

            auto worldCubeCenter = glm::inverse(lightView) * glm::vec4(cubeCenter, 1.0f);

            glm::vec3 scaleVec(squareSize, squareSize, lengths.z);
            glm::vec3 transVec(worldCubeCenter);

            m_outlineTransforms[i + 4].M = glm::translate(transVec) * glm::transpose(lightView) * glm::scale(scaleVec);
        }

        for (auto& trans : m_outlineTransforms)
        {
            trans.MV = camera.getViewMatrix() * trans.M;
            trans.MVP = camera.getProjectionMatrix() * trans.MV;
        }

        m_outlineTransformsBuffer->updateStagingBuffer(m_outlineTransforms.data(), m_outlineTransforms.size() * sizeof(Transforms));

    }

    void BoxVisualizer::updateDeviceBuffers(VkCommandBuffer commandBuffer, uint32_t frameIndex)
    {
        m_outlineTransformsBuffer->updateDeviceBuffer(commandBuffer, frameIndex);

        for (auto& frustum : m_frusta)
            frustum.vertexBuffer->updateDeviceBuffer(commandBuffer, frameIndex);
    }

    void BoxVisualizer::render(VkCommandBuffer commandBuffer, uint32_t frameIndex)
    {
        m_outlinePipeline->bind(commandBuffer);
        //m_cubeGeometry->bindGeometryBuffers(commandBuffer);

        std::vector<glm::vec4> colors =
        {
            { 1.0f, 0.0f, 0.0f, 1.0f },
            { 1.0f, 1.0f, 0.0f, 1.0f },
            { 0.0f, 1.0f, 0.0f, 1.0f },
            { 0.0f, 0.0f, 1.0f, 1.0f }
        };

        m_indexBuffer->bind(commandBuffer, 0);

        for (int i = 0; i < 4; i++)
        {
            m_frusta[i].vertexBindingGroup.offsets[0] = frameIndex * m_frustumPoints.size() * sizeof(glm::vec3);
            m_frusta[i].vertexBindingGroup.bind(commandBuffer);

            m_outlineDesc.setDynamicOffset(0, m_outlineTransformsBuffer->getDynamicOffset(frameIndex) + i * sizeof(Transforms));
            m_outlineDesc.bind(commandBuffer, m_outlinePipeline->getPipelineLayout());

            vkCmdPushConstants(commandBuffer, m_outlinePipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &colors[i]);

            //m_cubeGeometry->draw(commandBuffer);
            vkCmdDrawIndexed(commandBuffer, m_numIndices, 1, 0, 0, 0);
        }

        m_cubeVertexBindingGroup.bind(commandBuffer);
        for (int i = 4; i < 8; i++)
        {
            m_outlineDesc.setDynamicOffset(0, m_outlineTransformsBuffer->getDynamicOffset(frameIndex) + i * sizeof(Transforms));
            m_outlineDesc.bind(commandBuffer, m_outlinePipeline->getPipelineLayout());

            auto color = colors[i - 4] * 0.5f;
            color.a = 1.0f;
            vkCmdPushConstants(commandBuffer, m_outlinePipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &color);

            //m_cubeGeometry->draw(commandBuffer);
            vkCmdDrawIndexed(commandBuffer, m_numIndices, 1, 0, 0, 0);
        }
    }
}