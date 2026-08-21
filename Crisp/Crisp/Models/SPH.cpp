#include <Crisp/Models/SPH.hpp>

#include <algorithm>
#include <bit>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Renderer/ComputePipeline.hpp>

namespace crisp {
namespace {
constexpr uint32_t kScanBlockSize = 256;
constexpr uint32_t kScanElementsPerThread = 2;

constexpr float kSmoothingRadiusInParticleRadii = 4.0f;

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
    , m_substepCount(config.substepCount) {
    CRISP_CHECK_GT(m_numParticles, 0);
    CRISP_CHECK_GT(m_substepCount, 0);

    // The block of particles fills the box in x but only a quarter of it in y and z, so it has room
    // to collapse and spread instead of starting flush against the walls.
    m_fluidSpaceSize = glm::vec3(m_fluidDim.x, m_fluidDim.y / 2, m_fluidDim.z / 2) * 4.0f * m_particleRadius;

    m_cellSize = kSmoothingRadiusInParticleRadii * m_particleRadius;
    m_gridDim = glm::uvec3(glm::ceil(m_fluidSpaceSize / m_cellSize));
    m_numCells = m_gridDim.x * m_gridDim.y * m_gridDim.z;

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

    m_clearHashGrid = createDispatch("clear-hash-grid.comp", kLinearWorkGroup, perCell);
    m_clearHashGrid.material->writeDescriptor(0, 0, *m_cellCountBuffer);

    m_cellCount = createDispatch("compute-cell-count.comp", kLinearWorkGroup, perParticle);
    m_cellCount.material->writeDescriptor(0, 0, *m_positionBuffer);
    m_cellCount.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_cellCount.material->writeDescriptor(0, 2, *m_cellIdBuffer);

    m_scan = createDispatch("scan.comp", kLinearWorkGroup, VkExtent3D{m_scanBlockCount, 1, 1});
    m_scan.material->writeDescriptor(0, 0, *m_cellCountBuffer);
    m_scan.material->writeDescriptor(0, 1, *m_blockSumBuffer);

    m_scanBlock = createDispatch("scan.comp", {scanBlockWorkGroupSize, 1, 1}, VkExtent3D{1, 1, 1});
    m_scanBlock.material->writeDescriptor(0, 0, *m_blockSumBuffer);
    m_scanBlock.material->writeDescriptor(0, 1, *m_blockSumBuffer);

    m_scanCombine = createDispatch("scan-combine.comp", kLinearWorkGroup, perCell);
    m_scanCombine.material->writeDescriptor(0, 0, *m_cellCountBuffer);
    m_scanCombine.material->writeDescriptor(0, 1, *m_blockSumBuffer);

    m_reindex = createDispatch("reindex-particles.comp", kLinearWorkGroup, perParticle);
    m_reindex.material->writeDescriptor(0, 0, *m_positionBuffer);
    m_reindex.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_reindex.material->writeDescriptor(0, 2, *m_cellIdBuffer);
    m_reindex.material->writeDescriptor(0, 3, *m_sortedIndexBuffer);
    m_reindex.material->writeDescriptor(0, 4, *m_sortedPositionBuffer);

    m_densityPressure = createDispatch("compute-density-and-pressure.comp", kLinearWorkGroup, perParticle);
    m_densityPressure.material->writeDescriptor(0, 0, *m_positionBuffer);
    m_densityPressure.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_densityPressure.material->writeDescriptor(0, 2, *m_sortedPositionBuffer);
    m_densityPressure.material->writeDescriptor(0, 3, *m_densityBuffer);
    m_densityPressure.material->writeDescriptor(0, 4, *m_pressureBuffer);

    m_forces = createDispatch("compute-forces.comp", kLinearWorkGroup, perParticle);
    m_forces.material->writeDescriptor(0, 0, *m_positionBuffer);
    m_forces.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_forces.material->writeDescriptor(0, 2, *m_sortedIndexBuffer);
    m_forces.material->writeDescriptor(0, 3, *m_densityBuffer);
    m_forces.material->writeDescriptor(0, 4, *m_pressureBuffer);
    m_forces.material->writeDescriptor(0, 5, *m_velocityBuffer);
    m_forces.material->writeDescriptor(0, 6, *m_forceBuffer);

    m_integrate = createDispatch("integrate.comp", kLinearWorkGroup, perParticle);
    m_integrate.material->writeDescriptor(0, 0, *m_positionBuffer);
    m_integrate.material->writeDescriptor(0, 1, *m_velocityBuffer);
    m_integrate.material->writeDescriptor(0, 2, *m_forceBuffer);
    m_integrate.material->writeDescriptor(0, 3, *m_positionBuffer);
    m_integrate.material->writeDescriptor(0, 4, *m_velocityBuffer);
    m_integrate.material->writeDescriptor(0, 5, *m_colorBuffer);

    device.flushDescriptorUpdates();
}

SPH::~SPH() = default;

void SPH::update(const float dt) {
    const float frameTime = m_params.useRealFrameTime ? dt : m_params.simulatedTimePerFrame;
    m_substepTime = std::min(frameTime / static_cast<float>(m_substepCount), m_params.maxSubstepTime);
}

void SPH::addComputePasses(rg::RenderGraph& renderGraph) {
    // Each writing pass re-imports the buffer it modifies, producing a fresh handle that resolves to
    // the same VkBuffer. The graph keys its access history by VkBuffer, so threading the latest
    // handle forward is all the ordering the solver needs -- between substeps, and across frames.
    RenderGraphResourceHandle positions{};
    RenderGraphResourceHandle colors{};
    RenderGraphResourceHandle velocities{};
    RenderGraphResourceHandle forces{};
    RenderGraphResourceHandle densities{};
    RenderGraphResourceHandle pressures{};
    RenderGraphResourceHandle cellCounts{};
    RenderGraphResourceHandle cellIds{};
    RenderGraphResourceHandle sortedIndices{};
    RenderGraphResourceHandle sortedPositions{};
    RenderGraphResourceHandle blockSums{};

    const GridPushConstants grid{
        .dim = m_gridDim,
        .numCells = m_numCells,
        .spaceSize = m_fluidSpaceSize,
        .cellSize = m_cellSize,
    };

    const auto dispatch = [this](const Dispatch& d, const FrameContext& ctx, const auto& pushConstants) {
        if (m_isPaused) {
            return;
        }
        d.bind(ctx);
        ctx.commandEncoder.setPushConstants(
            *d.pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, pushConstants);
        ctx.commandEncoder.dispatchCompute(d.dispatchSize);
    };

    for (uint32_t substep = 0; substep < m_substepCount; ++substep) {
        const auto passName = [substep](const std::string_view stage) {
            return fmt::format("sph-{}-{}", substep, stage);
        };
        const auto resourceName = [substep](const std::string_view buffer, const std::string_view stage) {
            return fmt::format("sph-{}@{}-{}", buffer, substep, stage);
        };
        const bool isLastSubstep = substep + 1 == m_substepCount;

        renderGraph.addPass(
            passName("clear-hash-grid"),
            PassType::Compute,
            [this, &cellCounts, &resourceName](rg::RenderGraph::Builder& builder) {
                cellCounts =
                    builder.importBuffer(describe(*m_cellCountBuffer), resourceName("cell-counts", "clear-hash-grid"));
            },
            [this, dispatch](const FrameContext& ctx) { dispatch(m_clearHashGrid, ctx, m_numCells); });

        renderGraph.addPass(
            passName("cell-count"),
            PassType::Compute,
            [this, &positions, &cellCounts, &cellIds, &resourceName](rg::RenderGraph::Builder& builder) {
                positions = builder.importBuffer(describe(*m_positionBuffer), resourceName("positions", "cell-count"));
                builder.readBuffer(positions, kComputeRead);
                builder.readBuffer(cellCounts, kComputeRead);
                cellCounts =
                    builder.importBuffer(describe(*m_cellCountBuffer), resourceName("cell-counts", "cell-count"));
                cellIds = builder.importBuffer(describe(*m_cellIdBuffer), resourceName("cell-ids", "cell-count"));
            },
            [this, dispatch, grid](const FrameContext& ctx) {
                dispatch(
                    m_cellCount,
                    ctx,
                    CellCountPushConstants{.dim = grid.dim, .cellSize = grid.cellSize, .numParticles = m_numParticles});
            });

        renderGraph.addPass(
            passName("scan"),
            PassType::Compute,
            [this, &cellCounts, &blockSums, &resourceName](rg::RenderGraph::Builder& builder) {
                builder.readBuffer(cellCounts, kComputeRead);
                cellCounts = builder.importBuffer(describe(*m_cellCountBuffer), resourceName("cell-counts", "scan"));
                blockSums = builder.importBuffer(describe(*m_blockSumBuffer), resourceName("block-sums", "scan"));
            },
            [this, dispatch](const FrameContext& ctx) {
                dispatch(m_scan, ctx, ScanPushConstants{.storeSumBlocks = 1, .elementCount = m_numCells});
            });

        renderGraph.addPass(
            passName("scan-block"),
            PassType::Compute,
            [this, &blockSums, &resourceName](rg::RenderGraph::Builder& builder) {
                builder.readBuffer(blockSums, kComputeRead);
                blockSums = builder.importBuffer(describe(*m_blockSumBuffer), resourceName("block-sums", "scan-block"));
            },
            [this, dispatch](const FrameContext& ctx) {
                dispatch(m_scanBlock, ctx, ScanPushConstants{.storeSumBlocks = 0, .elementCount = m_scanBlockCount});
            });

        renderGraph.addPass(
            passName("scan-combine"),
            PassType::Compute,
            [this, &cellCounts, &blockSums, &resourceName](rg::RenderGraph::Builder& builder) {
                builder.readBuffer(blockSums, kComputeRead);
                builder.readBuffer(cellCounts, kComputeRead);
                cellCounts =
                    builder.importBuffer(describe(*m_cellCountBuffer), resourceName("cell-counts", "scan-combine"));
            },
            [this, dispatch](const FrameContext& ctx) {
                dispatch(
                    m_scanCombine,
                    ctx,
                    ScanCombinePushConstants{.elementCount = m_numCells, .elementsPerBlock = m_scanElementsPerBlock});
            });

        renderGraph.addPass(
            passName("reindex"),
            PassType::Compute,
            [this, &positions, &cellCounts, &cellIds, &sortedIndices, &sortedPositions, &resourceName](
                rg::RenderGraph::Builder& builder) {
                builder.readBuffer(positions, kComputeRead);
                builder.readBuffer(cellCounts, kComputeRead);
                builder.readBuffer(cellIds, kComputeRead);
                sortedIndices =
                    builder.importBuffer(describe(*m_sortedIndexBuffer), resourceName("sorted-indices", "reindex"));
                sortedPositions = builder.importBuffer(
                    describe(*m_sortedPositionBuffer), resourceName("sorted-positions", "reindex"));
            },
            [this, dispatch, grid](const FrameContext& ctx) {
                dispatch(m_reindex, ctx, ParticlePushConstants{.grid = grid, .numParticles = m_numParticles});
            });

        renderGraph.addPass(
            passName("density-pressure"),
            PassType::Compute,
            [this, &positions, &cellCounts, &sortedPositions, &densities, &pressures, &resourceName](
                rg::RenderGraph::Builder& builder) {
                builder.readBuffer(positions, kComputeRead);
                builder.readBuffer(cellCounts, kComputeRead);
                builder.readBuffer(sortedPositions, kComputeRead);
                densities =
                    builder.importBuffer(describe(*m_densityBuffer), resourceName("densities", "density-pressure"));
                pressures =
                    builder.importBuffer(describe(*m_pressureBuffer), resourceName("pressures", "density-pressure"));
            },
            [this, dispatch, grid](const FrameContext& ctx) {
                dispatch(m_densityPressure, ctx, ParticlePushConstants{.grid = grid, .numParticles = m_numParticles});
            });

        renderGraph.addPass(
            passName("forces"),
            PassType::Compute,
            [this, &positions, &cellCounts, &sortedIndices, &densities, &pressures, &velocities, &forces, &resourceName](
                rg::RenderGraph::Builder& builder) {
                builder.readBuffer(positions, kComputeRead);
                builder.readBuffer(cellCounts, kComputeRead);
                builder.readBuffer(sortedIndices, kComputeRead);
                builder.readBuffer(densities, kComputeRead);
                builder.readBuffer(pressures, kComputeRead);
                velocities = builder.importBuffer(describe(*m_velocityBuffer), resourceName("velocities", "forces"));
                builder.readBuffer(velocities, kComputeRead);
                forces = builder.importBuffer(describe(*m_forceBuffer), resourceName("forces", "forces"));
            },
            [this, dispatch, grid](const FrameContext& ctx) {
                dispatch(
                    m_forces,
                    ctx,
                    ForcesPushConstants{
                        .grid = grid,
                        .gravity = m_params.gravity,
                        .numParticles = m_numParticles,
                        .viscosity = m_params.viscosity,
                        .kappa = m_params.kappa,
                    });
            });

        renderGraph.addPass(
            passName("integrate"),
            PassType::Compute,
            [this, &positions, &velocities, &forces, &colors, &resourceName, isLastSubstep](
                rg::RenderGraph::Builder& builder) {
                builder.readBuffer(positions, kComputeRead);
                builder.readBuffer(velocities, kComputeRead);
                builder.readBuffer(forces, kComputeRead);
                positions = builder.importBuffer(describe(*m_positionBuffer), resourceName("positions", "integrate"));
                velocities = builder.importBuffer(describe(*m_velocityBuffer), resourceName("velocities", "integrate"));
                colors = builder.importBuffer(describe(*m_colorBuffer), resourceName("colors", "integrate"));

                if (isLastSubstep) {
                    auto& data = builder.getBlackboard().insert<SphPassData>();
                    data.positions = positions;
                    data.colors = colors;
                }
            },
            [this, dispatch, grid](const FrameContext& ctx) {
                dispatch(
                    m_integrate,
                    ctx,
                    IntegratePushConstants{.grid = grid, .timeDelta = m_substepTime, .numParticles = m_numParticles});
            });
    }
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
