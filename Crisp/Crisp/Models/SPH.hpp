#pragma once

#include <memory>
#include <vector>

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>

namespace crisp {

struct SphPassData {
    RenderGraphResourceHandle positions;
    RenderGraphResourceHandle colors;
};

struct SphParameters {
    glm::vec3 gravity{0.0f, -9.81f, 0.0f};
    float viscosity{5.0f};
    float kappa{1.0f}; // Surface tension coefficient.

    // Simulated seconds one rendered frame advances, split evenly across the substeps.
    float simulatedTimePerFrame{1.0f / 60.0f};
    // Take that from the real frame time instead, so the fluid tracks wall clock rather than frames.
    bool useRealFrameTime{false};

    // Ceiling on a single substep, so a hitch costs simulated time rather than the solver. The
    // Courant bound for the hardcoded stiffness is 0.4h/sqrt(stiffness) = 1.6 ms; stay under it.
    float maxSubstepTime{0.0015f};
};

struct SphConfig {
    glm::uvec3 fluidDim{32, 64, 32};
    float particleRadius{0.01f};
    // Solver iterations per rendered frame. 16 puts the default substep near 1 ms.
    uint32_t substepCount{16};
};

class SPH {
public:
    SPH(Renderer& renderer, const SphConfig& config);
    ~SPH();

    SPH(const SPH&) = delete;
    SPH& operator=(const SPH&) = delete;
    SPH(SPH&&) = delete;
    SPH& operator=(SPH&&) = delete;

    void addComputePasses(rg::RenderGraph& renderGraph);

    // Derives the substep size from the configured rate. Call once per frame, before render().
    void update(float dt);

    void reset();

    uint32_t getSubstepCount() const {
        return m_substepCount;
    }

    // Takes effect only once the render graph is rebuilt: the substep count is how many times the
    // solver chain is baked into it.
    void setSubstepCount(const uint32_t substepCount) {
        m_substepCount = substepCount;
    }

    float getSubstepTime() const {
        return m_substepTime;
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

    // The box the particles are confined to, in simulation units (metres).
    glm::vec3 getFluidSpaceSize() const {
        return m_fluidSpaceSize;
    }

    VulkanBuffer& getPositionBuffer() const {
        return *m_positionBuffer;
    }

    VulkanBuffer& getColorBuffer() const {
        return *m_colorBuffer;
    }

    SphParameters& getParameters() {
        return m_params;
    }

    const SphParameters& getParameters() const {
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
    std::vector<glm::vec4> createInitialPositions() const;

    Renderer& m_renderer;

    uint32_t m_numParticles;
    float m_particleRadius;
    glm::uvec3 m_fluidDim;
    glm::vec3 m_fluidSpaceSize;

    glm::uvec3 m_gridDim;
    uint32_t m_numCells;
    float m_cellSize;

    // Elements one `scan` workgroup reduces, and therefore the number of partial sums `scan-block`
    // has to scan in a single workgroup.
    uint32_t m_scanElementsPerBlock;
    uint32_t m_scanBlockCount;

    SphParameters m_params;
    uint32_t m_substepCount;
    float m_substepTime{0.0f};
    bool m_isPaused{false};

    std::unique_ptr<VulkanBuffer> m_positionBuffer;
    std::unique_ptr<VulkanBuffer> m_colorBuffer;
    std::unique_ptr<VulkanBuffer> m_velocityBuffer;
    std::unique_ptr<VulkanBuffer> m_forceBuffer;
    std::unique_ptr<VulkanBuffer> m_densityBuffer;
    std::unique_ptr<VulkanBuffer> m_pressureBuffer;

    std::unique_ptr<VulkanBuffer> m_cellCountBuffer;
    std::unique_ptr<VulkanBuffer> m_cellIdBuffer;
    std::unique_ptr<VulkanBuffer> m_sortedIndexBuffer;
    std::unique_ptr<VulkanBuffer> m_sortedPositionBuffer;
    std::unique_ptr<VulkanBuffer> m_blockSumBuffer;

    Dispatch m_clearHashGrid;
    Dispatch m_cellCount;
    Dispatch m_scan;
    Dispatch m_scanBlock;
    Dispatch m_scanCombine;
    Dispatch m_reindex;
    Dispatch m_densityPressure;
    Dispatch m_forces;
    Dispatch m_integrate;
};
} // namespace crisp
