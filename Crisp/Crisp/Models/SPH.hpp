#pragma once

#include <memory>

#include <Crisp/Math/Headers.hpp>

#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/StorageBuffer.hpp>
#include <Crisp/vulkan/VulkanBuffer.hpp>

#include "FluidSimulation.hpp"

namespace crisp
{
class VulkanPipeline;
class Renderer;
class VulkanDevice;
class UniformBuffer;
class RenderGraph;

class SPH : public FluidSimulation
{
public:
    SPH(Renderer* renderer, RenderGraph* renderGraph);
    virtual ~SPH();

    virtual void update(float dt) override;

    virtual void onKeyPressed(Key key, int modifier) override;
    virtual void setGravityX(float value) override;
    virtual void setGravityY(float value) override;
    virtual void setGravityZ(float value) override;
    virtual void setViscosity(float value) override;
    virtual void setSurfaceTension(float value) override;
    virtual void reset() override;

    virtual float getParticleRadius() const override;

    virtual VulkanBuffer* getVertexBuffer(std::string_view key) const override;

    virtual uint32_t getParticleCount() const override;
    virtual uint32_t getCurrentSection() const override;

private:
    std::vector<glm::vec4> createInitialPositions(glm::uvec3 fluidDim, float particleRadius) const;

    Renderer* m_renderer;
    std::unique_ptr<StorageBuffer> m_vertexBuffer;
    std::unique_ptr<StorageBuffer> m_colorBuffer;
    std::unique_ptr<StorageBuffer> m_indexBuffer;

    std::unique_ptr<StorageBuffer> m_reorderedPositionBuffer;

    std::unique_ptr<StorageBuffer> m_cellCountBuffer;
    std::unique_ptr<StorageBuffer> m_cellIdBuffer;
    std::unique_ptr<StorageBuffer> m_blockSumBuffer;
    uint32_t m_blockSumRegionSize;

    std::unique_ptr<StorageBuffer> m_densityBuffer;
    std::unique_ptr<StorageBuffer> m_pressureBuffer;
    std::unique_ptr<StorageBuffer> m_velocityBuffer;
    std::unique_ptr<StorageBuffer> m_forcesBuffer;

    float m_particleRadius;
    glm::uvec3 m_fluidDim;

    float m_timeDelta;

    mutable uint32_t m_prevSection;
    mutable uint32_t m_currentSection;

    struct GridParams
    {
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