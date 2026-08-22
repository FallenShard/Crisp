#include <Crisp/Models/SPH.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Renderer/ComputePipeline.hpp>

namespace crisp {
namespace {
constexpr uint32_t kScanBlockSize = 256;
constexpr uint32_t kScanElementsPerThread = 2;

constexpr float kSmoothingRadiusInParticleRadii = 4.0f;

// Order matters: it is the order recordSolve dispatches them in, and the order the timings and the
// ui report them in.
constexpr std::array<const char*, 9> kStageNames{
    "clear-hash-grid",
    "cell-count",
    "scan",
    "scan-block",
    "scan-combine",
    "reindex",
    "density-pressure",
    "forces",
    "integrate",
};

struct GridPushConstants {
    glm::uvec3 dim;
    uint32_t numCells;
    glm::vec3 spaceSize;
    float cellSize;
};

static_assert(sizeof(GridPushConstants) == 32);

struct CellCountPushConstants {
    glm::uvec3 dim;
    float cellSize;
    uint32_t numParticles;
};

struct ScanPushConstants {
    int32_t storeSumBlocks;
    uint32_t elementCount;
};

struct ScanCombinePushConstants {
    uint32_t elementCount;
    uint32_t elementsPerBlock;
};

struct ParticlePushConstants {
    GridPushConstants grid;
    uint32_t numParticles;
};

struct ForcesPushConstants {
    GridPushConstants grid;
    glm::vec3 gravity;
    uint32_t numParticles;
    float viscosity;
    float kappa;
};

static_assert(sizeof(ForcesPushConstants) == 56);

struct IntegratePushConstants {
    GridPushConstants grid;
    float timeDelta;
    uint32_t numParticles;
};

VkExtent3D linearDispatch(const uint32_t itemCount, const uint32_t workGroupSize) {
    return {(itemCount + workGroupSize - 1) / workGroupSize, 1, 1};
}

RenderGraphBufferDescription describe(const VulkanBuffer& buffer) {
    return {
        .formatHint = VK_FORMAT_UNDEFINED,
        .size = buffer.getSize(),
        .usageFlags = 0,
        .externalBuffer = buffer.getHandle(),
    };
}

std::unique_ptr<VulkanBuffer> createParticleBuffer(
    const VulkanDevice& device, const VkDeviceSize size, const VkBufferUsageFlags2 extraUsage = 0) {
    return std::make_unique<VulkanBuffer>(
        device,
        size,
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | extraUsage,
        BufferMemoryType::GpuOnly);
}
} // namespace

void SPH::Dispatch::bind(const FrameContext& ctx) const {
    ctx.commandEncoder.bindPipeline(*pipeline);
    ctx.commandEncoder.bindDescriptorSets(material->getDescriptorSetBinding());
}

SPH::Dispatch SPH::createDispatch(
    const std::string& shaderName, const VkExtent3D& workGroupSize, const VkExtent3D dispatchSize) const {
    Dispatch dispatch{};
    dispatch.dispatchSize = dispatchSize;
    dispatch.pipeline = createComputePipeline(
        m_renderer.getDevice(), m_renderer.getAssetPaths().getShaderSpvPath(shaderName), workGroupSize);
    dispatch.material = std::make_unique<Material>(dispatch.pipeline.get());
    dispatch.material->setDebugName(shaderName);
    return dispatch;
}

SPH::SPH(Renderer& renderer, const SphConfig& config)
    : m_renderer(renderer)
    , m_numParticles(config.fluidDim.x * config.fluidDim.y * config.fluidDim.z)
    , m_particleRadius(config.particleRadius)
    , m_fluidDim(config.fluidDim)
    , m_fluidSpaceSize(glm::vec3(m_fluidDim.x, m_fluidDim.y / 2, m_fluidDim.z / 2) * 4.0f * m_particleRadius)
    , m_cellSize(kSmoothingRadiusInParticleRadii * m_particleRadius)
    , m_gridDim(glm::uvec3(glm::ceil(m_fluidSpaceSize / m_cellSize)))
    , m_numCells(m_gridDim.x * m_gridDim.y * m_gridDim.z)
    , m_maxSubstepCount(config.maxSubstepCount) {
    CRISP_CHECK_GT(m_numParticles, 0);
    CRISP_CHECK_GT(m_maxSubstepCount, 0);

    m_scanElementsPerBlock = kScanBlockSize * kScanElementsPerThread;
    m_scanBlockCount = (m_numCells + m_scanElementsPerBlock - 1) / m_scanElementsPerBlock;
    // scan-block reduces every partial sum in a single workgroup, and its Blelloch pass needs a
    // power-of-two element count.
    const uint32_t scanBlockWorkGroupSize = std::max(std::bit_ceil(m_scanBlockCount) / 2, 1u);
    CRISP_CHECK_LE(
        scanBlockWorkGroupSize, m_renderer.getDevice().getPhysicalDevice().getLimits().maxComputeWorkGroupInvocations);

    const VkDeviceSize vec4BufferSize = m_numParticles * sizeof(glm::vec4);
    auto& device = m_renderer.getDevice();

    m_positionBuffer = createParticleBuffer(device, vec4BufferSize, VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT);
    m_colorBuffer = createParticleBuffer(device, vec4BufferSize, VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT);
    m_velocityBuffer = createParticleBuffer(device, vec4BufferSize);
    m_forceBuffer = createParticleBuffer(device, vec4BufferSize);
    m_sortedPositionBuffer = createParticleBuffer(device, vec4BufferSize);
    m_densityBuffer = createParticleBuffer(device, m_numParticles * sizeof(float));
    m_pressureBuffer = createParticleBuffer(device, m_numParticles * sizeof(float));
    m_cellIdBuffer = createParticleBuffer(device, m_numParticles * sizeof(uint32_t));
    m_sortedIndexBuffer = createParticleBuffer(device, m_numParticles * sizeof(uint32_t));
    m_cellCountBuffer = createParticleBuffer(device, m_numCells * sizeof(uint32_t));
    m_blockSumBuffer = createParticleBuffer(device, m_scanBlockCount * sizeof(uint32_t));

    device.setObjectName(m_positionBuffer->getHandle(), "sph-positions");
    device.setObjectName(m_colorBuffer->getHandle(), "sph-colors");

    reset();

    constexpr VkExtent3D kLinearWorkGroup{kScanBlockSize, 1, 1};
    const auto perParticle = linearDispatch(m_numParticles, kScanBlockSize);
    const auto perCell = linearDispatch(m_numCells, kScanBlockSize);

    m_clearHashGrid = createDispatch("Sph/clear-hash-grid.comp", kLinearWorkGroup, perCell);
    m_clearHashGrid.material->writeDescriptor(0, 0, *m_cellCountBuffer);

    m_cellCount = createDispatch("Sph/compute-cell-count.comp", kLinearWorkGroup, perParticle);
    m_cellCount.material->writeDescriptor(0, 0, *m_positionBuffer);
    m_cellCount.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_cellCount.material->writeDescriptor(0, 2, *m_cellIdBuffer);

    m_scan = createDispatch("Sph/scan.comp", kLinearWorkGroup, VkExtent3D{m_scanBlockCount, 1, 1});
    m_scan.material->writeDescriptor(0, 0, *m_cellCountBuffer);
    m_scan.material->writeDescriptor(0, 1, *m_blockSumBuffer);

    m_scanBlock = createDispatch("Sph/scan.comp", {scanBlockWorkGroupSize, 1, 1}, VkExtent3D{1, 1, 1});
    m_scanBlock.material->writeDescriptor(0, 0, *m_blockSumBuffer);
    m_scanBlock.material->writeDescriptor(0, 1, *m_blockSumBuffer);

    m_scanCombine = createDispatch("Sph/scan-combine.comp", kLinearWorkGroup, perCell);
    m_scanCombine.material->writeDescriptor(0, 0, *m_cellCountBuffer);
    m_scanCombine.material->writeDescriptor(0, 1, *m_blockSumBuffer);

    m_reindex = createDispatch("Sph/reindex-particles.comp", kLinearWorkGroup, perParticle);
    m_reindex.material->writeDescriptor(0, 0, *m_positionBuffer);
    m_reindex.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_reindex.material->writeDescriptor(0, 2, *m_cellIdBuffer);
    m_reindex.material->writeDescriptor(0, 3, *m_sortedIndexBuffer);
    m_reindex.material->writeDescriptor(0, 4, *m_sortedPositionBuffer);

    m_densityPressure = createDispatch("Sph/compute-density-and-pressure.comp", kLinearWorkGroup, perParticle);
    m_densityPressure.material->writeDescriptor(0, 0, *m_positionBuffer);
    m_densityPressure.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_densityPressure.material->writeDescriptor(0, 2, *m_sortedPositionBuffer);
    m_densityPressure.material->writeDescriptor(0, 3, *m_densityBuffer);
    m_densityPressure.material->writeDescriptor(0, 4, *m_pressureBuffer);

    m_forces = createDispatch("Sph/compute-forces.comp", kLinearWorkGroup, perParticle);
    m_forces.material->writeDescriptor(0, 0, *m_positionBuffer);
    m_forces.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_forces.material->writeDescriptor(0, 2, *m_sortedIndexBuffer);
    m_forces.material->writeDescriptor(0, 3, *m_densityBuffer);
    m_forces.material->writeDescriptor(0, 4, *m_pressureBuffer);
    m_forces.material->writeDescriptor(0, 5, *m_velocityBuffer);
    m_forces.material->writeDescriptor(0, 6, *m_forceBuffer);

    m_integrate = createDispatch("Sph/integrate.comp", kLinearWorkGroup, perParticle);
    m_integrate.material->writeDescriptor(0, 0, *m_positionBuffer);
    m_integrate.material->writeDescriptor(0, 1, *m_velocityBuffer);
    m_integrate.material->writeDescriptor(0, 2, *m_forceBuffer);
    m_integrate.material->writeDescriptor(0, 3, *m_positionBuffer);
    m_integrate.material->writeDescriptor(0, 4, *m_velocityBuffer);
    m_integrate.material->writeDescriptor(0, 5, *m_colorBuffer);

    m_stageProfiler.initialize(device, kStageNames.size(), "SPH Solver Stages");

    device.flushDescriptorUpdates();
}

SPH::~SPH() = default;

void SPH::update(const float dt) {
    const float frameTime = m_params.useFixedFrameTime ? m_params.simulatedTimePerFrame : dt * m_params.timeScale;
    const auto requiredSubsteps = static_cast<uint32_t>(std::ceil(frameTime / m_params.targetSubstepTime));
    m_activeSubstepCount = std::clamp(requiredSubsteps, 1u, m_maxSubstepCount);
    m_substepTime = std::min(frameTime / static_cast<float>(m_activeSubstepCount), m_params.targetSubstepTime);
}

std::span<const char* const> SPH::getStageNames() {
    return kStageNames;
}

void SPH::addComputePasses(rg::RenderGraph& renderGraph) {
    renderGraph.addPass(
        "sph-solve",
        PassType::Compute,
        [this](rg::RenderGraph::Builder& builder) {
            auto& data = builder.getBlackboard().insert<WcsphPassData>();
            data.positions = builder.importBuffer(describe(*m_positionBuffer), "sph-positions");
            data.colors = builder.importBuffer(describe(*m_colorBuffer), "sph-colors");
        },
        [this](const FrameContext& ctx) { recordSolve(ctx); });
}

void SPH::recordSolve(const FrameContext& ctx) {
    if (m_isPaused || m_activeSubstepCount == 0) {
        return;
    }

    const auto& encoder = ctx.commandEncoder;
    constexpr auto kStageBarrier = kComputeWrite >> (kComputeRead | kComputeWrite);
    encoder.insertBarrier((kComputeWrite | kVertexInputRead) >> (kComputeRead | kComputeWrite));

    const GridPushConstants grid{
        .dim = m_gridDim,
        .numCells = m_numCells,
        .spaceSize = m_fluidSpaceSize,
        .cellSize = m_cellSize,
    };

    m_stageProfiler.beginFrame(ctx.virtualFrameIndex);
    for (uint32_t substep = 0; substep < m_activeSubstepCount; ++substep) {
        const bool isTimed = substep == 0;
        uint32_t stage = 0;
        const auto runStage = [&](const Dispatch& dispatch, const auto& pushConstants) {
            if (isTimed) {
                m_stageProfiler.beginPass(encoder, stage);
            }
            dispatch.bind(ctx);
            encoder.setPushConstants(
                *dispatch.pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, pushConstants);
            encoder.dispatchCompute(dispatch.dispatchSize);
            if (isTimed) {
                m_stageProfiler.endPass(encoder, stage);
            }
            encoder.insertBarrier(kStageBarrier);
            ++stage;
        };

        runStage(m_clearHashGrid, m_numCells);
        runStage(
            m_cellCount,
            CellCountPushConstants{.dim = grid.dim, .cellSize = grid.cellSize, .numParticles = m_numParticles});
        runStage(m_scan, ScanPushConstants{.storeSumBlocks = 1, .elementCount = m_numCells});
        runStage(m_scanBlock, ScanPushConstants{.storeSumBlocks = 0, .elementCount = m_scanBlockCount});
        runStage(
            m_scanCombine,
            ScanCombinePushConstants{.elementCount = m_numCells, .elementsPerBlock = m_scanElementsPerBlock});
        runStage(m_reindex, ParticlePushConstants{.grid = grid, .numParticles = m_numParticles});
        runStage(m_densityPressure, ParticlePushConstants{.grid = grid, .numParticles = m_numParticles});
        runStage(
            m_forces,
            ForcesPushConstants{
                .grid = grid,
                .gravity = m_params.gravity,
                .numParticles = m_numParticles,
                .viscosity = m_params.viscosity,
                .kappa = m_params.kappa,
            });
        runStage(
            m_integrate,
            IntegratePushConstants{.grid = grid, .timeDelta = m_substepTime, .numParticles = m_numParticles});
    }
    m_stageProfiler.endFrame();
}

void SPH::reset() {
    m_renderer.finish();

    const VkDeviceSize vec4BufferSize = m_numParticles * sizeof(glm::vec4);
    const auto positions = createInitialPositions();
    fillDeviceBuffer(m_renderer, m_positionBuffer.get(), positions.data(), vec4BufferSize);

    const std::vector<glm::vec4> velocities(m_numParticles, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    fillDeviceBuffer(m_renderer, m_velocityBuffer.get(), velocities.data(), vec4BufferSize);

    const std::vector<glm::vec4> colors(m_numParticles, glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));
    fillDeviceBuffer(m_renderer, m_colorBuffer.get(), colors.data(), vec4BufferSize);
}

std::vector<glm::vec4> SPH::createInitialPositions() const {
    std::vector<glm::vec4> positions;
    positions.reserve(m_numParticles);
    for (uint32_t z = 0; z < m_fluidDim.z; ++z) {
        for (uint32_t y = 0; y < m_fluidDim.y; ++y) {
            for (uint32_t x = 0; x < m_fluidDim.x; ++x) {
                const glm::vec3 pos = glm::vec3(x, y, z) * 2.0f * m_particleRadius + m_particleRadius;
                positions.emplace_back(pos, 1.0f);
            }
        }
    }
    return positions;
}
} // namespace crisp
