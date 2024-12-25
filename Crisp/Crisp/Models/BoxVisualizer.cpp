#include <Crisp/Models/BoxVisualizer.hpp>

#include <Crisp/Camera/FreeCameraController.hpp>

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>

namespace crisp {
BoxVisualizer::BoxVisualizer(Renderer* renderer, uint32_t numBoxes, const VulkanRenderPass& renderPass)
    : m_renderer(renderer)
    , m_boxes(numBoxes)
    , m_transforms(numBoxes, glm::mat4(1.0f)) {
    std::vector<glm::uvec2> lines = {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},

        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4},

        {0, 4},
        {3, 7},
        {2, 6},
        {1, 5}};

    m_indexGeometry = std::make_unique<Geometry>(*renderer, std::vector<glm::vec4>{}, lines);

    m_outlinePipeline = m_renderer->createPipeline("DebugBox.lua", renderPass, 0);
    m_outlineMaterial = std::make_unique<Material>(m_outlinePipeline.get());

    for (int i = 0; i < m_boxes.size(); ++i) {
        glm::vec4 translation((i / 2) * 5.0f, 0.0f, (i % 2) * 5.0f, 0.0f);

        m_boxes[i].points[0] = glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f);
        m_boxes[i].points[1] = glm::vec4(+1.0f, -1.0f, 1.0f, 1.0f);
        m_boxes[i].points[2] = glm::vec4(+1.0f, +1.0f, 1.0f, 1.0f);
        m_boxes[i].points[3] = glm::vec4(-1.0f, +1.0f, 1.0f, 1.0f);

        m_boxes[i].points[4] = glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f);
        m_boxes[i].points[5] = glm::vec4(+1.0f, -1.0f, -1.0f, 1.0f);
        m_boxes[i].points[6] = glm::vec4(+1.0f, +1.0f, -1.0f, 1.0f);
        m_boxes[i].points[7] = glm::vec4(-1.0f, +1.0f, -1.0f, 1.0f);

        for (auto& p : m_boxes[i].points) {
            p += translation;
        }

        m_boxes[i].color = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
    }
}

BoxVisualizer::~BoxVisualizer() {}

void BoxVisualizer::setBoxCorners(uint32_t i, const std::array<glm::vec3, 8>& corners) {
    for (uint32_t j = 0; j < corners.size(); ++j) {
        m_boxes[i].points[j] = glm::vec4(corners[j], 1.0f);
    }
}

void BoxVisualizer::setBoxColor(uint32_t i, glm::vec4 color) {
    m_boxes[i].color = color;
}

void BoxVisualizer::update(const glm::mat4& V, const glm::mat4& P) {
    for (uint32_t i = 0; i < m_boxes.size(); ++i) {
        m_boxes[i].transform = P * V * m_transforms[i];
    }
}

std::vector<RenderNode> BoxVisualizer::createRenderNodes() const {
    std::vector<RenderNode> nodes(m_boxes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        // nodes[i].geometry = m_indexGeometry.get();
        // nodes[i].material = nullptr;
        // nodes[i].pipeline = m_outlinePipeline.get();
        // nodes[i].setPushConstantView(m_boxes[i]);
    }
    return nodes;
}
} // namespace crisp