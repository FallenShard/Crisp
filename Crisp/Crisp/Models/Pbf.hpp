#pragma once

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/PassProfiler.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>

namespace crisp {

struct PbfPassData {
    RenderGraphResourceHandle positions;
    RenderGraphResourceHandle colors;
};

struct PbfParameters {
    glm::vec3 gravity{0.0f, -9.81f, 0.0f};

    float compliance{1.0e-6f};
    float sCorrK{0.01f};
    float sCorrN{4.0f};
    float sCorrDqOverH{0.2f};

    float xsphC{0.05f};

    float timeScale{1.0f};

    bool useFixedFrameTime{false};
    float simulatedTimePerFrame{1.0f / 60.0f};

    float targetSubstepTime{0.0008f};

    float colorSpeedScale{0.25f};
};

struct PbfConfig {
    glm::uvec3 fluidDim{32, 64, 32};
    float particleRadius{0.01f};
    uint32_t maxSubstepCount{32};
};

class PositionBasedFluid {
public:
    PositionBasedFluid(Renderer& renderer, const PbfConfig& config);
    ~PositionBasedFluid();

    PositionBasedFluid(const PositionBasedFluid&) = delete;
    PositionBasedFluid& operator=(const PositionBasedFluid&) = delete;
    PositionBasedFluid(PositionBasedFluid&&) = delete;
    PositionBasedFluid& operator=(PositionBasedFluid&&) = delete;

    void addComputePasses(rg::RenderGraph& renderGraph);
    void update(float dt);
    void reset();

    uint32_t getMaxSubstepCount() const {
        return m_maxSubstepCount;
    }

    void setMaxSubstepCount(const uint32_t maxSubstepCount) {
        m_maxSubstepCount = maxSubstepCount;
    }

    uint32_t getActiveSubstepCount() const {
        return m_activeSubstepCount;
    }

    static std::span<const char* const> getStageNames();

    std::span<const std::optional<double>> getStageTimingsMs() const {
        return m_stageProfiler.getPassTimingsMs();
    }

    float getSubstepTime() const {
        return m_substepTime;
    }

    float getSimulatedTimePerFrame() const {
        return m_substepTime * static_cast<float>(m_activeSubstepCount);
    }

    bool isPaused() const {
        return m_isPaused;
    }

    void setPaused(const bool paused) {
        m_isPaused = paused;
    }

    uint32_t getParticleCount() const {
        return m_numParticles;
    }

    float getParticleRadius() const {
        return m_particleRadius;
    }

    glm::vec3 getFluidSpaceSize() const {
        return m_fluidSpaceSize;
    }

    float getParticleMass() const {
        return m_particleMass;
    }

    VulkanBuffer& getPositionBuffer() const {
        return *m_positionBuffer;
    }

    VulkanBuffer& getColorBuffer() const {
        return *m_colorBuffer;
    }

    VulkanBuffer& getDensityBuffer() const {
        return *m_densityBuffer;
    }

    PbfParameters& getParameters() {
        return m_params;
    }

    const PbfParameters& getParameters() const {
        return m_params;
    }

private:
    struct Dispatch {
        std::unique_ptr<VulkanPipeline> pipeline;
        std::unique_ptr<Material> material;
        VkExtent3D dispatchSize{};

        void bind(const FrameContext& ctx) const;
    };

    Dispatch createDispatch(
        const std::string& shaderName, const VkExtent3D& workGroupSize, VkExtent3D dispatchSize) const;

    void recordSolve(const FrameContext& ctx);
    std::vector<glm::vec4> createInitialPositions() const;

    Renderer& m_renderer;

    uint32_t m_numParticles;
    float m_particleRadius;
    float m_particleMass;
    glm::uvec3 m_fluidDim;
    glm::vec3 m_fluidSpaceSize;

    glm::uvec3 m_gridDim;
    uint32_t m_numCells;
    float m_cellSize;

    float m_latticeSumGradC2{1.0f};

    uint32_t m_scanElementsPerBlock;
    uint32_t m_scanBlockCount;

    PbfParameters m_params;
    uint32_t m_maxSubstepCount;
    uint32_t m_activeSubstepCount{1};
    float m_substepTime{0.0f};
    bool m_isPaused{false};

    std::unique_ptr<VulkanBuffer> m_positionBuffer;
    std::unique_ptr<VulkanBuffer> m_colorBuffer;
    std::unique_ptr<VulkanBuffer> m_velocityBuffer;
    std::unique_ptr<VulkanBuffer> m_predictedPositionBuffer;

    std::unique_ptr<VulkanBuffer> m_sortedPositionBuffer;
    std::unique_ptr<VulkanBuffer> m_sortedVelocityBuffer;
    std::unique_ptr<VulkanBuffer> m_sortedIndexBuffer;
    std::unique_ptr<VulkanBuffer> m_densityBuffer;
    std::unique_ptr<VulkanBuffer> m_lambdaBuffer;
    std::unique_ptr<VulkanBuffer> m_deltaPositionBuffer;

    std::unique_ptr<VulkanBuffer> m_cellCountBuffer;
    std::unique_ptr<VulkanBuffer> m_cellIdBuffer;
    std::unique_ptr<VulkanBuffer> m_blockSumBuffer;

    PassProfiler m_stageProfiler;

    Dispatch m_predict;
    Dispatch m_clearHashGrid;
    Dispatch m_cellCount;
    Dispatch m_scan;
    Dispatch m_scanBlock;
    Dispatch m_scanCombine;
    Dispatch m_reindex;
    Dispatch m_lambda;
    Dispatch m_delta;
    Dispatch m_apply;
    Dispatch m_viscosity;
};
} // namespace crisp
