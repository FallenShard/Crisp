#include <Crisp/Models/Pbf.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <numbers>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Renderer/ComputePipeline.hpp>

namespace crisp {
namespace {
constexpr uint32_t kScanBlockSize = 256;
constexpr uint32_t kScanElementsPerThread = 2;

constexpr float kSmoothingRadiusInParticleRadii = 4.0f;

constexpr float kRestDensity = 1000.0f;

constexpr std::array<const char*, 11> kStageNames{
    "predict",
    "clear-hash-grid",
    "cell-count",
    "scan",
    "scan-block",
    "scan-combine",
    "reindex",
    "lambda",
    "delta",
    "apply",
    "viscosity",
};

// Must match PbfParams in Shaders/Common/pbf.part.glsl, field for field. Nothing checks it.
struct PbfPushConstants {
    glm::uvec3 gridDim;
    uint32_t numCells;

    glm::vec3 spaceSize;
    float cellSize;

    glm::vec3 gravity;
    float h;

    float mass;
    float restDensity;
    uint32_t numParticles;
    float dt;

    float alphaTilde;
    float sCorrK;
    float sCorrN;
    float sCorrDenom;

    float xsphC;
    float particleRadius;
    float colorSpeedScale;
    float pad;
};

static_assert(sizeof(PbfPushConstants) == 96);

struct ScanPushConstants {
    int32_t storeSumBlocks;
    uint32_t elementCount;
};

struct ScanCombinePushConstants {
    uint32_t elementCount;
    uint32_t elementsPerBlock;
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
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT |
            extraUsage,
        BufferMemoryType::GpuOnly);
}

// Must match sphPoly6 in Shaders/Common/pbf.part.glsl.
double poly6(const double dist2, const double h) {
    const double h2 = h * h;
    if (dist2 >= h2) {
        return 0.0;
    }
    const double val = h2 - dist2;
    const double h3 = h2 * h;
    return 315.0 / (64.0 * std::numbers::pi * h3 * h3 * h3) * val * val * val;
}

// Must match sphSpikyGrad. Returns the magnitude; the direction is the separation, normalized.
double spikyGradMagnitude(const double dist, const double h) {
    if (dist >= h || dist <= 0.0) {
        return 0.0;
    }
    const double h2 = h * h;
    return -45.0 / (std::numbers::pi * h2 * h2 * h2) * (h - dist) * (h - dist);
}

struct LatticeReference {
    double density;
    double sumGradC2;
};

LatticeReference computeLatticeReference(
    const double spacing, const double h, const double mass, const double restDensity) {
    const auto extent = static_cast<int32_t>(h / spacing) + 2;
    const double h2 = h * h;

    double kernelSum = 0.0;
    glm::dvec3 gradSum{0.0};
    double gradSqSum = 0.0;
    for (int32_t i = -extent; i <= extent; ++i) {
        for (int32_t j = -extent; j <= extent; ++j) {
            for (int32_t k = -extent; k <= extent; ++k) {
                const glm::dvec3 offset{i * spacing, j * spacing, k * spacing};
                const double dist2 = glm::dot(offset, offset);
                if (dist2 >= h2) {
                    continue;
                }
                kernelSum += poly6(dist2, h);

                const double dist = std::sqrt(dist2);
                const double magnitude = spikyGradMagnitude(dist, h);
                if (magnitude != 0.0) {
                    const glm::dvec3 grad = magnitude * offset / dist;
                    gradSum += grad;
                    gradSqSum += glm::dot(grad, grad);
                }
            }
        }
    }

    const double scale = mass / restDensity;
    return {
        .density = mass * kernelSum,
        .sumGradC2 = scale * scale * (glm::dot(gradSum, gradSum) + gradSqSum),
    };
}
} // namespace

void PositionBasedFluid::Dispatch::bind(const FrameContext& ctx) const {
    ctx.commandEncoder.bindPipeline(*pipeline);
    ctx.commandEncoder.bindDescriptorSets(material->getDescriptorSetBinding());
}

PositionBasedFluid::Dispatch PositionBasedFluid::createDispatch(
    const std::string& shaderName, const VkExtent3D& workGroupSize, const VkExtent3D dispatchSize) const {
    Dispatch dispatch{};
    dispatch.dispatchSize = dispatchSize;
    dispatch.pipeline = createComputePipeline(
        m_renderer.getDevice(), m_renderer.getAssetPaths().getShaderSpvPath(shaderName), workGroupSize);
    dispatch.material = std::make_unique<Material>(dispatch.pipeline.get());
    dispatch.material->setDebugName(shaderName);
    return dispatch;
}

PositionBasedFluid::PositionBasedFluid(Renderer& renderer, const PbfConfig& config)
    : m_renderer(renderer)
    , m_numParticles(config.fluidDim.x * config.fluidDim.y * config.fluidDim.z)
    , m_particleRadius(config.particleRadius)
    , m_fluidDim(config.fluidDim)
    , m_maxSubstepCount(config.maxSubstepCount) {
    CRISP_CHECK_GT(m_numParticles, 0);
    CRISP_CHECK_GT(m_maxSubstepCount, 0);

    const float spacing = 2.0f * m_particleRadius;
    m_particleMass = kRestDensity * spacing * spacing * spacing;

    m_fluidSpaceSize = glm::vec3(m_fluidDim.x, m_fluidDim.y / 2, m_fluidDim.z / 2) * 4.0f * m_particleRadius;

    m_cellSize = kSmoothingRadiusInParticleRadii * m_particleRadius;
    m_gridDim = glm::uvec3(glm::ceil(m_fluidSpaceSize / m_cellSize));
    m_numCells = m_gridDim.x * m_gridDim.y * m_gridDim.z;

    const auto lattice = computeLatticeReference(spacing, m_cellSize, m_particleMass, kRestDensity);
    m_latticeSumGradC2 = static_cast<float>(lattice.sumGradC2);
    CRISP_CHECK_LE(std::abs(lattice.density - kRestDensity) / kRestDensity, 0.05);

    m_scanElementsPerBlock = kScanBlockSize * kScanElementsPerThread;
    m_scanBlockCount = (m_numCells + m_scanElementsPerBlock - 1) / m_scanElementsPerBlock;
    const uint32_t scanBlockWorkGroupSize = std::max(std::bit_ceil(m_scanBlockCount) / 2, 1u);
    CRISP_CHECK_LE(
        scanBlockWorkGroupSize, m_renderer.getDevice().getPhysicalDevice().getLimits().maxComputeWorkGroupInvocations);

    const VkDeviceSize vec4BufferSize = m_numParticles * sizeof(glm::vec4);
    auto& device = m_renderer.getDevice();

    m_positionBuffer = createParticleBuffer(device, vec4BufferSize, VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT);
    m_colorBuffer = createParticleBuffer(device, vec4BufferSize, VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT);
    m_velocityBuffer = createParticleBuffer(device, vec4BufferSize);
    m_predictedPositionBuffer = createParticleBuffer(device, vec4BufferSize);

    m_sortedPositionBuffer = createParticleBuffer(device, vec4BufferSize);
    m_sortedVelocityBuffer = createParticleBuffer(device, vec4BufferSize);
    m_deltaPositionBuffer = createParticleBuffer(device, vec4BufferSize);
    m_sortedIndexBuffer = createParticleBuffer(device, m_numParticles * sizeof(uint32_t));
    m_densityBuffer = createParticleBuffer(device, m_numParticles * sizeof(float));
    m_lambdaBuffer = createParticleBuffer(device, m_numParticles * sizeof(float));

    m_cellIdBuffer = createParticleBuffer(device, m_numParticles * sizeof(uint32_t));
    m_cellCountBuffer = createParticleBuffer(device, m_numCells * sizeof(uint32_t));
    m_blockSumBuffer = createParticleBuffer(device, m_scanBlockCount * sizeof(uint32_t));

    device.setObjectName(m_positionBuffer->getHandle(), "sph-positions");
    device.setObjectName(m_colorBuffer->getHandle(), "sph-colors");

    reset();

    constexpr VkExtent3D kLinearWorkGroup{kScanBlockSize, 1, 1};
    const auto perParticle = linearDispatch(m_numParticles, kScanBlockSize);
    const auto perCell = linearDispatch(m_numCells, kScanBlockSize);

    m_predict = createDispatch("Pbf/predict.comp", kLinearWorkGroup, perParticle);
    m_predict.material->writeDescriptor(0, 0, *m_positionBuffer);
    m_predict.material->writeDescriptor(0, 1, *m_velocityBuffer);
    m_predict.material->writeDescriptor(0, 2, *m_predictedPositionBuffer);

    m_clearHashGrid = createDispatch("Pbf/clear-hash-grid.comp", kLinearWorkGroup, perCell);
    m_clearHashGrid.material->writeDescriptor(0, 0, *m_cellCountBuffer);

    m_cellCount = createDispatch("Pbf/cell-count.comp", kLinearWorkGroup, perParticle);
    m_cellCount.material->writeDescriptor(0, 0, *m_predictedPositionBuffer);
    m_cellCount.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_cellCount.material->writeDescriptor(0, 2, *m_cellIdBuffer);

    m_scan = createDispatch("Pbf/scan.comp", kLinearWorkGroup, VkExtent3D{m_scanBlockCount, 1, 1});
    m_scan.material->writeDescriptor(0, 0, *m_cellCountBuffer);
    m_scan.material->writeDescriptor(0, 1, *m_blockSumBuffer);

    m_scanBlock = createDispatch("Pbf/scan.comp", {scanBlockWorkGroupSize, 1, 1}, VkExtent3D{1, 1, 1});
    m_scanBlock.material->writeDescriptor(0, 0, *m_blockSumBuffer);
    m_scanBlock.material->writeDescriptor(0, 1, *m_blockSumBuffer);

    m_scanCombine = createDispatch("Pbf/scan-combine.comp", kLinearWorkGroup, perCell);
    m_scanCombine.material->writeDescriptor(0, 0, *m_cellCountBuffer);
    m_scanCombine.material->writeDescriptor(0, 1, *m_blockSumBuffer);

    m_reindex = createDispatch("Pbf/reindex.comp", kLinearWorkGroup, perParticle);
    m_reindex.material->writeDescriptor(0, 0, *m_predictedPositionBuffer);
    m_reindex.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_reindex.material->writeDescriptor(0, 2, *m_cellIdBuffer);
    m_reindex.material->writeDescriptor(0, 3, *m_sortedIndexBuffer);
    m_reindex.material->writeDescriptor(0, 4, *m_sortedPositionBuffer);

    m_lambda = createDispatch("Pbf/lambda.comp", kLinearWorkGroup, perParticle);
    m_lambda.material->writeDescriptor(0, 0, *m_sortedPositionBuffer);
    m_lambda.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_lambda.material->writeDescriptor(0, 2, *m_densityBuffer);
    m_lambda.material->writeDescriptor(0, 3, *m_lambdaBuffer);

    m_delta = createDispatch("Pbf/delta.comp", kLinearWorkGroup, perParticle);
    m_delta.material->writeDescriptor(0, 0, *m_sortedPositionBuffer);
    m_delta.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_delta.material->writeDescriptor(0, 2, *m_lambdaBuffer);
    m_delta.material->writeDescriptor(0, 3, *m_deltaPositionBuffer);

    m_apply = createDispatch("Pbf/apply.comp", kLinearWorkGroup, perParticle);
    m_apply.material->writeDescriptor(0, 0, *m_sortedPositionBuffer);
    m_apply.material->writeDescriptor(0, 1, *m_deltaPositionBuffer);
    m_apply.material->writeDescriptor(0, 2, *m_sortedIndexBuffer);
    m_apply.material->writeDescriptor(0, 3, *m_positionBuffer);
    m_apply.material->writeDescriptor(0, 4, *m_sortedVelocityBuffer);

    m_viscosity = createDispatch("Pbf/viscosity.comp", kLinearWorkGroup, perParticle);
    m_viscosity.material->writeDescriptor(0, 0, *m_sortedPositionBuffer);
    m_viscosity.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    m_viscosity.material->writeDescriptor(0, 2, *m_sortedVelocityBuffer);
    m_viscosity.material->writeDescriptor(0, 3, *m_densityBuffer);
    m_viscosity.material->writeDescriptor(0, 4, *m_sortedIndexBuffer);
    m_viscosity.material->writeDescriptor(0, 5, *m_velocityBuffer);
    m_viscosity.material->writeDescriptor(0, 6, *m_colorBuffer);

    m_stageProfiler.initialize(device, kStageNames.size(), "PBF Solver Stages");

    device.flushDescriptorUpdates();
}

PositionBasedFluid::~PositionBasedFluid() = default;

void PositionBasedFluid::update(const float dt) {
    const float frameTime = m_params.useFixedFrameTime ? m_params.simulatedTimePerFrame : dt * m_params.timeScale;

    const auto required = static_cast<uint32_t>(std::ceil(frameTime / m_params.targetSubstepTime));
    m_activeSubstepCount = std::clamp(required, 1u, m_maxSubstepCount);
    m_substepTime = std::min(frameTime / static_cast<float>(m_activeSubstepCount), m_params.targetSubstepTime);
}

std::span<const char* const> PositionBasedFluid::getStageNames() {
    return kStageNames;
}

void PositionBasedFluid::addComputePasses(rg::RenderGraph& renderGraph) {
    renderGraph.addPass(
        "pbf-solve",
        PassType::Compute,
        [this](rg::RenderGraph::Builder& builder) {
            auto& data = builder.getBlackboard().insert<PbfPassData>();
            data.positions = builder.importBuffer(describe(*m_positionBuffer), "pbf-positions");
            data.colors = builder.importBuffer(describe(*m_colorBuffer), "pbf-colors");
        },
        [this](const FrameContext& ctx) { recordSolve(ctx); });
}

void PositionBasedFluid::recordSolve(const FrameContext& ctx) {
    if (m_isPaused || m_activeSubstepCount == 0) {
        return;
    }

    const auto& encoder = ctx.commandEncoder;

    constexpr auto kStageBarrier = kComputeWrite >> (kComputeRead | kComputeWrite);
    encoder.insertBarrier((kComputeWrite | kVertexInputRead) >> (kComputeRead | kComputeWrite));

    const float dt = m_substepTime;
    const float sCorrDq = m_params.sCorrDqOverH * m_cellSize;
    const PbfPushConstants constants{
        .gridDim = m_gridDim,
        .numCells = m_numCells,
        .spaceSize = m_fluidSpaceSize,
        .cellSize = m_cellSize,
        .gravity = m_params.gravity,
        .h = m_cellSize,
        .mass = m_particleMass,
        .restDensity = kRestDensity,
        .numParticles = m_numParticles,
        .dt = dt,
        .alphaTilde = m_params.compliance / std::max(dt * dt, 1e-12f),
        .sCorrK = m_params.sCorrK / m_latticeSumGradC2,
        .sCorrN = m_params.sCorrN,
        .sCorrDenom = static_cast<float>(std::max(poly6(sCorrDq * sCorrDq, m_cellSize), 1e-20)),
        .xsphC = m_params.xsphC,
        .particleRadius = m_particleRadius,
        .colorSpeedScale = m_params.colorSpeedScale,
        .pad = 0.0f,
    };

    m_stageProfiler.beginFrame(ctx.virtualFrameIndex);
    for (uint32_t substep = 0; substep < m_activeSubstepCount; ++substep) {
        const bool isTimed = substep == 0;
        uint32_t stage = 0;
        const auto runStage = [&](const Dispatch& dispatch, const auto& pushConstantData) {
            if (isTimed) {
                m_stageProfiler.beginPass(encoder, stage);
            }
            dispatch.bind(ctx);
            encoder.setPushConstants(
                *dispatch.pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, pushConstantData);
            encoder.dispatchCompute(dispatch.dispatchSize);
            if (isTimed) {
                m_stageProfiler.endPass(encoder, stage);
            }
            encoder.insertBarrier(kStageBarrier);
            ++stage;
        };

        runStage(m_predict, constants);
        runStage(m_clearHashGrid, constants);
        runStage(m_cellCount, constants);
        runStage(m_scan, ScanPushConstants{.storeSumBlocks = 1, .elementCount = m_numCells});
        runStage(m_scanBlock, ScanPushConstants{.storeSumBlocks = 0, .elementCount = m_scanBlockCount});
        runStage(
            m_scanCombine,
            ScanCombinePushConstants{.elementCount = m_numCells, .elementsPerBlock = m_scanElementsPerBlock});
        runStage(m_reindex, constants);
        runStage(m_lambda, constants);
        runStage(m_delta, constants);
        runStage(m_apply, constants);
        runStage(m_viscosity, constants);
    }
    m_stageProfiler.endFrame();
}

void PositionBasedFluid::reset() {
    m_renderer.finish();

    const VkDeviceSize vec4BufferSize = m_numParticles * sizeof(glm::vec4);
    const auto positions = createInitialPositions();
    fillDeviceBuffer(m_renderer, m_positionBuffer.get(), positions.data(), vec4BufferSize);
    fillDeviceBuffer(m_renderer, m_predictedPositionBuffer.get(), positions.data(), vec4BufferSize);

    const std::vector<glm::vec4> velocities(m_numParticles, glm::vec4(0.0f));
    fillDeviceBuffer(m_renderer, m_velocityBuffer.get(), velocities.data(), vec4BufferSize);

    const std::vector<glm::vec4> colors(m_numParticles, glm::vec4(0.05f, 0.15f, 0.45f, 1.0f));
    fillDeviceBuffer(m_renderer, m_colorBuffer.get(), colors.data(), vec4BufferSize);
}

std::vector<glm::vec4> PositionBasedFluid::createInitialPositions() const {
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
