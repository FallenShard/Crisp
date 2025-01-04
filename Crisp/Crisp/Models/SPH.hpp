#pragma once

#include <memory>

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Models/FluidSimulation.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>

namespace crisp {
class VulkanPipeline;
class Renderer;
class VulkanDevice;
class RenderGraph;

class SPH : public FluidSimulation {
public:
    SPH(Renderer* renderer, RenderGraph* renderGraph);

    void update(float dt) override;

    void onKeyPressed(Key key, int modifier) override;
    void setGravityX(float value) override;
    void setGravityY(float value) override;
    void setGravityZ(float value) override;
    void setViscosity(float value) override;
    void setSurfaceTension(float value) override;
    void reset() override;

    float getParticleRadius() const override;

    VulkanBuffer* getVertexBuffer(std::string_view key) const override;

    uint32_t getParticleCount() const override;
    uint32_t getCurrentSection() const override;

private:
    std::vector<glm::vec4> createInitialPositions(glm::uvec3 fluidDim, float particleRadius) const;

    Renderer* m_renderer;
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;
    std::unique_ptr<VulkanBuffer> m_colorBuffer;
    std::unique_ptr<VulkanBuffer> m_indexBuffer;

    std::unique_ptr<VulkanBuffer> m_reorderedPositionBuffer;

    std::unique_ptr<VulkanBuffer> m_cellCountBuffer;
    std::unique_ptr<VulkanBuffer> m_cellIdBuffer;
    std::unique_ptr<VulkanBuffer> m_blockSumBuffer;
    uint32_t m_blockSumRegionSize;

    std::unique_ptr<VulkanBuffer> m_densityBuffer;
    std::unique_ptr<VulkanBuffer> m_pressureBuffer;
    std::unique_ptr<VulkanBuffer> m_velocityBuffer;
    std::unique_ptr<VulkanBuffer> m_forcesBuffer;

    float m_particleRadius;
    glm::uvec3 m_fluidDim;

    float m_timeDelta;

    mutable uint32_t m_prevSection;
    mutable uint32_t m_currentSection;

    struct GridParams {
        glm::uvec3 dim;
        uint32_t numCells;
        glm::vec3 spaceSize;
        float cellSize;
    };

    glm::vec3 m_fluidSpaceMin;
    glm::vec3 m_fluidSpaceMax;
    GridParams m_gridParams;

    uint32_t m_numParticles;

    float m_viscosityFactor = 5.0f;
    float m_kappa = 1.0f;
    glm::vec3 m_gravity = {0.0f, -9.81f, 0.0f};
    bool m_runSimulation = false;

    RenderGraph* m_renderGraph;
};
} // namespace crisp