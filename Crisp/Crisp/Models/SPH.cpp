#include "SPH.hpp"

#include <Crisp/Renderer/Renderer.hpp>

#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>

namespace crisp
{
namespace
{
static constexpr uint32_t ScanBlockSize = 256;
static constexpr uint32_t ScanElementsPerThread = 2;
static constexpr uint32_t ScanElementsPerBlock = ScanBlockSize * ScanElementsPerThread;

inline glm::uvec3 getNumWorkGroups(const glm::uvec3& dataDims, const VulkanPipeline& pipeline)
{
    return (dataDims - glm::uvec3(1)) / getWorkGroupSize(pipeline) + glm::uvec3(1);
}

void setDispatchLayout(RenderGraph::Node& computeNode, const glm::ivec3& workGroupSize, const glm::ivec3& items)
{
    computeNode.workGroupSize = workGroupSize;
    computeNode.numWorkGroups = (items - 1) / workGroupSize + 1;
}

void createMaterial(
    RenderGraph::Node& computeNode,
    Renderer* renderer,
    std::string shaderName,
    uint32_t dynamicBuffers,
    uint32_t sets = 1)
{
    computeNode.pipeline =
        createComputePipeline(renderer, std::move(shaderName), dynamicBuffers, sets, computeNode.workGroupSize);
    computeNode.material = std::make_unique<Material>(computeNode.pipeline.get());
}

} // namespace

SPH::SPH(Renderer* renderer, RenderGraph* renderGraph)
    : m_renderer(renderer)
    , m_particleRadius(0.01f)
    , m_currentSection(0)
    , m_prevSection(0)
    , m_timeDelta(0.0f)
    , m_renderGraph(renderGraph)
    , m_runSimulation{true}
{
    m_fluidDim = glm::uvec3(32, 64, 32);
    m_numParticles = m_fluidDim.x * m_fluidDim.y * m_fluidDim.z;

    const VkDeviceSize vertexBufferSize = m_numParticles * sizeof(glm::vec4);
    m_vertexBuffer = std::make_unique<StorageBuffer>(
        m_renderer, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const std::vector<glm::vec4> positions = createInitialPositions(m_fluidDim, m_particleRadius);
    m_renderer->fillDeviceBuffer(m_vertexBuffer->getDeviceBuffer(), positions.data(), vertexBufferSize, 0);
    m_renderer->fillDeviceBuffer(
        m_vertexBuffer->getDeviceBuffer(), positions.data(), vertexBufferSize, vertexBufferSize);

    m_colorBuffer = std::make_unique<StorageBuffer>(
        m_renderer, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const auto colors = std::vector<glm::vec4>(m_numParticles, glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));
    m_renderer->fillDeviceBuffer(m_colorBuffer->getDeviceBuffer(), colors.data(), vertexBufferSize, 0);
    m_renderer->fillDeviceBuffer(m_colorBuffer->getDeviceBuffer(), colors.data(), vertexBufferSize, vertexBufferSize);

    m_reorderedPositionBuffer = std::make_unique<StorageBuffer>(m_renderer, vertexBufferSize);

    m_fluidSpaceMin = glm::vec3(0.0f);
    //// m_fluidSpaceMax = glm::vec3(m_fluidDim) * 2.0f * 2.0f * m_particleRadius;
    m_fluidSpaceMax = glm::vec3(m_fluidDim.x, m_fluidDim.y / 2, m_fluidDim.z / 2) * 4.0f * m_particleRadius;
    m_gridParams.cellSize = 4 * m_particleRadius;
    m_gridParams.spaceSize = m_fluidSpaceMax - m_fluidSpaceMin;
    m_gridParams.dim = glm::ivec3(glm::ceil((m_fluidSpaceMax - m_fluidSpaceMin) / m_gridParams.cellSize));
    m_gridParams.numCells = m_gridParams.dim.x * m_gridParams.dim.y * m_gridParams.dim.z;

    m_cellCountBuffer = std::make_unique<StorageBuffer>(m_renderer, m_gridParams.numCells * sizeof(uint32_t));
    m_cellIdBuffer = std::make_unique<StorageBuffer>(m_renderer, m_numParticles * sizeof(uint32_t));
    m_indexBuffer = std::make_unique<StorageBuffer>(m_renderer, m_numParticles * sizeof(uint32_t));

    m_blockSumRegionSize =
        static_cast<uint32_t>(std::max(m_gridParams.numCells / ScanElementsPerBlock * sizeof(uint32_t), 32ull));
    m_blockSumBuffer = std::make_unique<StorageBuffer>(m_renderer, m_blockSumRegionSize);

    m_densityBuffer = std::make_unique<StorageBuffer>(m_renderer, m_numParticles * sizeof(float));
    m_pressureBuffer = std::make_unique<StorageBuffer>(m_renderer, m_numParticles * sizeof(float));
    m_velocityBuffer = std::make_unique<StorageBuffer>(m_renderer, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    auto velocities = std::vector<glm::vec4>(m_numParticles, glm::vec4(glm::vec3(0.0f), 1.0f));
    m_renderer->fillDeviceBuffer(m_velocityBuffer->getDeviceBuffer(), velocities.data(), vertexBufferSize, 0);
    m_forcesBuffer = std::make_unique<StorageBuffer>(m_renderer, vertexBufferSize);

    // Clear Hash Grid
    auto& clearHashGrid = renderGraph->addComputePass("clear-hash-grid");
    setDispatchLayout(clearHashGrid, glm::ivec3(256, 1, 1), glm::ivec3(m_gridParams.numCells, 1, 1));
    createMaterial(clearHashGrid, m_renderer, "clear-hash-grid.comp", 1);

    // Output
    clearHashGrid.material->writeDescriptor(0, 0, *m_cellCountBuffer);

    clearHashGrid.preDispatchCallback =
        [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t frameIdx)
    {
        node.pipeline->setPushConstants(cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams.numCells);
        node.material->setDynamicOffset(frameIdx, 0, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        node.isEnabled = false;
    };

    renderGraph->addDependency(
        "clear-hash-grid",
        "compute-cell-count",
        [this](const VulkanRenderPass& /*src*/, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            cmdBuffer.insertBufferMemoryBarrier(
                m_cellCountBuffer->createSpanFromSection(m_currentSection),
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_READ_BIT);
        });

    // Compute particle count per cell
    auto& computeCellCount = renderGraph->addComputePass("compute-cell-count");
    computeCellCount.isEnabled = false;
    setDispatchLayout(computeCellCount, glm::ivec3(256, 1, 1), glm::ivec3(m_numParticles, 1, 1));
    createMaterial(computeCellCount, m_renderer, "compute-cell-count.comp", 3);

    // Input
    computeCellCount.material->writeDescriptor(0, 0, *m_vertexBuffer);

    // Output
    computeCellCount.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    computeCellCount.material->writeDescriptor(0, 2, *m_cellIdBuffer);

    computeCellCount.preDispatchCallback =
        [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t frameIdx)
    {
        node.pipeline->setPushConstants(
            cmdBuffer.getHandle(),
            VK_SHADER_STAGE_COMPUTE_BIT,
            m_gridParams.dim,
            m_gridParams.cellSize,
            m_numParticles);

        node.material->setDynamicOffset(frameIdx, 0, m_vertexBuffer->getDynamicOffset(m_prevSection));
        node.material->setDynamicOffset(frameIdx, 1, m_cellCountBuffer->getDynamicOffset(m_currentSection));
        node.material->setDynamicOffset(frameIdx, 2, m_cellIdBuffer->getDynamicOffset(m_currentSection));

        node.isEnabled = false;
    };

    renderGraph->addDependency(
        "compute-cell-count",
        "scan",
        [this](const VulkanRenderPass& /*src*/, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            std::array<VkBufferMemoryBarrier, 2> barriers;
            for (auto& barrier : barriers)
            {
                barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = m_gridParams.numCells * sizeof(uint32_t);
                barrier.offset = m_currentSection * m_gridParams.numCells * sizeof(uint32_t);
            }
            barriers[0].buffer = m_cellCountBuffer->getHandle();

            barriers[1].size = m_numParticles * sizeof(uint32_t);
            barriers[1].offset = m_currentSection * m_numParticles * sizeof(uint32_t);
            barriers[1].buffer = m_cellIdBuffer->getHandle();
            cmdBuffer.insertBufferMemoryBarriers(
                barriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });

    // Scan for individual blocks
    auto& scan = renderGraph->addComputePass("scan");
    scan.isEnabled = false;
    setDispatchLayout(scan, glm::ivec3(256, 1, 1), glm::ivec3(m_gridParams.numCells / ScanElementsPerThread, 1, 1));
    scan.pipeline = createComputePipeline(m_renderer, "scan.comp", 2, 1, scan.workGroupSize);
    scan.material = std::make_unique<Material>(scan.pipeline.get());

    scan.preDispatchCallback = [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t frameIdx)
    {
        node.pipeline->setPushConstants(cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, 1, m_gridParams.numCells);

        node.material->setDynamicOffset(frameIdx, 0, m_cellCountBuffer->getDynamicOffset(m_currentSection));
        node.material->setDynamicOffset(frameIdx, 1, m_blockSumBuffer->getDynamicOffset(m_currentSection));

        node.isEnabled = false;
    };

    // Input/Output
    scan.material->writeDescriptor(0, 0, *m_cellCountBuffer);
    scan.material->writeDescriptor(0, 1, *m_blockSumBuffer);

    renderGraph->addDependency(
        "scan",
        "scan-block",
        [this](const VulkanRenderPass& /*src*/, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            cmdBuffer.insertBufferMemoryBarrier(
                m_blockSumBuffer->createSpanFromSection(m_currentSection),
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_READ_BIT);
        });

    // Scan for the block sums
    auto& scanBlock = renderGraph->addComputePass("scan-block");
    scanBlock.isEnabled = false;
    setDispatchLayout(
        scanBlock,
        glm::ivec3(256, 1, 1),
        glm::ivec3(m_gridParams.numCells / ScanElementsPerBlock / ScanElementsPerThread, 1, 1));
    scanBlock.pipeline = createComputePipeline(m_renderer, "scan.comp", 2, 1, scanBlock.workGroupSize);
    scanBlock.material = std::make_unique<Material>(scanBlock.pipeline.get());

    // Input/Output
    scanBlock.material->writeDescriptor(0, 0, *m_blockSumBuffer);
    scanBlock.material->writeDescriptor(0, 1, *m_blockSumBuffer);
    scanBlock.preDispatchCallback = [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t frameIdx)
    {
        node.pipeline->setPushConstants(
            cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, 0, m_gridParams.numCells / ScanElementsPerBlock);

        node.material->setDynamicOffset(frameIdx, 0, m_blockSumBuffer->getDynamicOffset(m_currentSection));
        node.material->setDynamicOffset(frameIdx, 1, m_blockSumBuffer->getDynamicOffset(m_currentSection));

        node.isEnabled = false;
    };
    renderGraph->addDependency(
        "scan-block",
        "scan-combine",
        [this](const VulkanRenderPass& /*src*/, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            std::array<VkBufferMemoryBarrier, 1> barriers;
            for (auto& barrier : barriers)
            {
                barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = m_blockSumRegionSize;
                barrier.offset = m_currentSection * m_blockSumRegionSize;
            }
            barriers[0].buffer = m_blockSumBuffer->getHandle();

            cmdBuffer.insertBufferMemoryBarriers(
                barriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });

    // Add block prefix sum to intra-block prefix sums
    auto& scanCombine = renderGraph->addComputePass("scan-combine");
    scanCombine.isEnabled = false;
    setDispatchLayout(scanCombine, glm::ivec3(512, 1, 1), glm::ivec3(m_gridParams.numCells, 1, 1));
    scanCombine.pipeline = createComputePipeline(m_renderer, "scan-combine.comp", 2, 1, scanCombine.workGroupSize);
    scanCombine.material = std::make_unique<Material>(scanCombine.pipeline.get());

    // Input/Output
    scanCombine.material->writeDescriptor(0, 0, *m_cellCountBuffer);
    scanCombine.material->writeDescriptor(0, 1, *m_blockSumBuffer);
    scanCombine.preDispatchCallback = [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t frameIdx)
    {
        node.pipeline->setPushConstants(cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams.numCells);

        node.material->setDynamicOffset(frameIdx, 0, m_cellCountBuffer->getDynamicOffset(m_currentSection));
        node.material->setDynamicOffset(frameIdx, 1, m_blockSumBuffer->getDynamicOffset(m_currentSection));

        node.isEnabled = false;
    };

    renderGraph->addDependency(
        "scan-combine",
        "reindex", //"reindex",
        [this](const VulkanRenderPass& /*src*/, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            std::array<VkBufferMemoryBarrier, 2> barriers;
            for (auto& barrier : barriers)
            {
                barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = m_gridParams.numCells * sizeof(uint32_t);
                barrier.offset = m_currentSection * m_gridParams.numCells * sizeof(uint32_t);
            }
            barriers[0].buffer = m_cellCountBuffer->getHandle();

            barriers[1].size = m_blockSumRegionSize;
            barriers[1].offset = m_currentSection * m_blockSumRegionSize;
            barriers[1].buffer = m_blockSumBuffer->getHandle();

            cmdBuffer.insertBufferMemoryBarriers(
                barriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });

    auto& reindex = renderGraph->addComputePass("reindex");
    reindex.isEnabled = false;
    setDispatchLayout(reindex, glm::ivec3(256, 1, 1), glm::ivec3(m_numParticles, 1, 1));
    reindex.pipeline = createComputePipeline(m_renderer, "reindex-particles.comp", 5, 1, reindex.workGroupSize);
    reindex.material = std::make_unique<Material>(reindex.pipeline.get());

    // Input
    reindex.material->writeDescriptor(0, 0, {m_vertexBuffer->getHandle(), 0, vertexBufferSize});
    reindex.material->writeDescriptor(
        0, 1, {m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t)});
    reindex.material->writeDescriptor(0, 2, {m_cellIdBuffer->getHandle(), 0, m_numParticles * sizeof(uint32_t)});

    // Output
    reindex.material->writeDescriptor(0, 3, {m_indexBuffer->getHandle(), 0, m_numParticles * sizeof(uint32_t)});
    reindex.material->writeDescriptor(0, 4, {m_reorderedPositionBuffer->getHandle(), 0, vertexBufferSize});

    reindex.preDispatchCallback = [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t frameIdx)
    {
        node.pipeline->setPushConstants(
            cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, m_numParticles);

        node.material->setDynamicOffset(frameIdx, 0, m_prevSection * m_numParticles * sizeof(glm::vec4));

        node.material->setDynamicOffset(frameIdx, 1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        node.material->setDynamicOffset(frameIdx, 2, m_currentSection * m_numParticles * sizeof(uint32_t));
        node.material->setDynamicOffset(frameIdx, 3, m_currentSection * m_numParticles * sizeof(uint32_t));
        node.material->setDynamicOffset(frameIdx, 4, m_currentSection * m_numParticles * sizeof(glm::vec4));

        node.isEnabled = false;
    };
    renderGraph->addDependency(
        "reindex",
        "compute-density-and-pressure",
        [this](const VulkanRenderPass& /*src*/, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            std::array<VkBufferMemoryBarrier, 2> barriers;
            for (auto& barrier : barriers)
            {
                barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = m_numParticles * sizeof(uint32_t);
                barrier.offset = m_currentSection * m_numParticles * sizeof(uint32_t);
            }
            barriers[0].buffer = m_indexBuffer->getHandle();
            barriers[1].size = m_numParticles * sizeof(glm::vec4);
            barriers[1].offset = m_currentSection * m_numParticles * sizeof(glm::vec4);
            barriers[1].buffer = m_reorderedPositionBuffer->getHandle();
            cmdBuffer.insertBufferMemoryBarriers(
                barriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });

    auto& computePressure = renderGraph->addComputePass("compute-density-and-pressure");
    computePressure.isEnabled = false;
    setDispatchLayout(computePressure, glm::ivec3(256, 1, 1), glm::ivec3(m_numParticles, 1, 1));
    computePressure.pipeline =
        createComputePipeline(m_renderer, "compute-density-and-pressure.comp", 5, 1, computePressure.workGroupSize);
    computePressure.material = std::make_unique<Material>(computePressure.pipeline.get());

    // Input
    computePressure.material->writeDescriptor(0, 0, {m_vertexBuffer->getHandle(), 0, vertexBufferSize});
    computePressure.material->writeDescriptor(
        0, 1, {m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t)});
    computePressure.material->writeDescriptor(0, 2, {m_reorderedPositionBuffer->getHandle(), 0, vertexBufferSize});

    // Output
    computePressure.material->writeDescriptor(0, 3, {m_densityBuffer->getHandle(), 0, m_numParticles * sizeof(float)});
    computePressure.material->writeDescriptor(0, 4, {m_pressureBuffer->getHandle(), 0, m_numParticles * sizeof(float)});
    computePressure.preDispatchCallback =
        [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t frameIdx)
    {
        node.pipeline->setPushConstants(
            cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, m_numParticles);

        node.material->setDynamicOffset(frameIdx, 0, m_prevSection * m_numParticles * sizeof(glm::vec4));

        node.material->setDynamicOffset(frameIdx, 1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        node.material->setDynamicOffset(frameIdx, 2, m_currentSection * m_numParticles * sizeof(glm::vec4));
        node.material->setDynamicOffset(frameIdx, 3, m_currentSection * m_numParticles * sizeof(float));
        node.material->setDynamicOffset(frameIdx, 4, m_currentSection * m_numParticles * sizeof(float));

        node.isEnabled = false;
    };

    renderGraph->addDependency(
        "compute-density-and-pressure",
        "compute-forces",
        [this](const VulkanRenderPass& /*src*/, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            std::array<VkBufferMemoryBarrier, 2> barriers;
            for (auto& barrier : barriers)
            {
                barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = m_numParticles * sizeof(float);
                barrier.offset = m_currentSection * m_numParticles * sizeof(float);
            }
            barriers[0].buffer = m_densityBuffer->getHandle();
            barriers[1].buffer = m_pressureBuffer->getHandle();
            cmdBuffer.insertBufferMemoryBarriers(
                barriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });

    auto& computeForces = renderGraph->addComputePass("compute-forces");
    computeForces.isEnabled = false;
    setDispatchLayout(computeForces, glm::ivec3(256, 1, 1), glm::ivec3(m_numParticles, 1, 1));
    computeForces.pipeline =
        createComputePipeline(m_renderer, "compute-forces.comp", 7, 1, computeForces.workGroupSize);
    computeForces.material = std::make_unique<Material>(computeForces.pipeline.get());

    // Input
    computeForces.material->writeDescriptor(0, 0, *m_vertexBuffer);
    computeForces.material->writeDescriptor(0, 1, *m_cellCountBuffer);
    computeForces.material->writeDescriptor(0, 2, *m_indexBuffer);
    computeForces.material->writeDescriptor(0, 3, *m_densityBuffer);
    computeForces.material->writeDescriptor(0, 4, *m_pressureBuffer);
    computeForces.material->writeDescriptor(0, 5, *m_velocityBuffer);

    // Output
    computeForces.material->writeDescriptor(0, 6, *m_forcesBuffer);
    computeForces.preDispatchCallback =
        [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t frameIdx)
    {
        node.pipeline->setPushConstants(
            cmdBuffer.getHandle(),
            VK_SHADER_STAGE_COMPUTE_BIT,
            m_gridParams,
            m_gravity,
            m_numParticles,
            m_viscosityFactor,
            m_kappa);

        node.material->setDynamicOffset(frameIdx, 0, m_vertexBuffer->getDynamicOffset(m_prevSection));
        node.material->setDynamicOffset(frameIdx, 1, m_cellCountBuffer->getDynamicOffset(m_currentSection));
        node.material->setDynamicOffset(frameIdx, 2, m_indexBuffer->getDynamicOffset(m_currentSection));
        node.material->setDynamicOffset(frameIdx, 3, m_densityBuffer->getDynamicOffset(m_currentSection));
        node.material->setDynamicOffset(frameIdx, 4, m_pressureBuffer->getDynamicOffset(m_currentSection));
        node.material->setDynamicOffset(frameIdx, 5, m_velocityBuffer->getDynamicOffset(m_prevSection));
        node.material->setDynamicOffset(frameIdx, 6, m_forcesBuffer->getDynamicOffset(m_currentSection));

        node.isEnabled = false;
    };

    renderGraph->addDependency(
        "compute-forces",
        "integrate",
        [this,
         vertexBufferSize](const VulkanRenderPass& /*src*/, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            cmdBuffer.insertBufferMemoryBarrier(
                m_forcesBuffer->createSpanFromSection(m_currentSection),
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_READ_BIT);
        });

    auto& integrateNode = renderGraph->addComputePass("integrate");
    integrateNode.isEnabled = false;
    setDispatchLayout(integrateNode, glm::ivec3(256, 1, 1), glm::ivec3(m_numParticles, 1, 1));
    integrateNode.pipeline = createComputePipeline(m_renderer, "integrate.comp", 6, 1, integrateNode.workGroupSize);
    integrateNode.material = std::make_unique<Material>(integrateNode.pipeline.get());

    // .createComputePass("integrate.comp", ...);
    // .setWorkgroupSize(wgsize);
    // .setComputeItems(items);

    // setInputBuffer(0, 0, m_vertexBuffer, prevSection);
    // setInputBuffer(0, 1, m_velocityBuffer, prevSection);
    // setInputBuffer(0, 2, m_forcesBuffer, currSection);

    // setOutputBuffer(0, 3, m_vertexBuffer, currSection);
    // setOutputBuffer(0, 4, m_velocityBuffer, currSection);
    // setOutputBuffer(0, 5, m_colorBuffer, currSection);

    // auto configure preDispatchCallback to set material buffer dynamic offsets
    // auto configure dependency to insert barrier on outputs
    // synchronizeBufferOutputs({3, 5})

    // Input
    integrateNode.material->writeDescriptor(0, 0, *m_vertexBuffer);
    integrateNode.material->writeDescriptor(0, 1, *m_velocityBuffer);
    integrateNode.material->writeDescriptor(0, 2, *m_forcesBuffer);

    // Output
    integrateNode.material->writeDescriptor(0, 3, *m_vertexBuffer);
    integrateNode.material->writeDescriptor(0, 4, *m_velocityBuffer);
    integrateNode.material->writeDescriptor(0, 5, *m_colorBuffer);
    integrateNode.preDispatchCallback =
        [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t frameIdx)
    {
        node.pipeline->setPushConstants(
            cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, m_timeDelta / 2.0f, m_numParticles);

        node.material->setDynamicOffset(frameIdx, 0, m_vertexBuffer->getDynamicOffset(m_prevSection));
        node.material->setDynamicOffset(frameIdx, 1, m_velocityBuffer->getDynamicOffset(m_prevSection));
        node.material->setDynamicOffset(frameIdx, 2, m_forcesBuffer->getDynamicOffset(m_currentSection));
        node.material->setDynamicOffset(frameIdx, 3, m_vertexBuffer->getDynamicOffset(m_currentSection));
        node.material->setDynamicOffset(frameIdx, 4, m_velocityBuffer->getDynamicOffset(m_currentSection));
        node.material->setDynamicOffset(frameIdx, 5, m_colorBuffer->getDynamicOffset(m_currentSection));

        node.isEnabled = false;
    };

    renderGraph->addDependency(
        "integrate",
        "mainPass",
        [this,
         vertexBufferSize](const VulkanRenderPass& /*src*/, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            std::array<VkBufferMemoryBarrier, 2> barriers;
            for (auto& barrier : barriers)
            {
                barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
                barrier.size = vertexBufferSize;
                barrier.offset = m_currentSection * vertexBufferSize;
            }
            barriers[0].buffer = m_vertexBuffer->getHandle();
            barriers[1].buffer = m_colorBuffer->getHandle();
            cmdBuffer.insertBufferMemoryBarriers(
                barriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
        });
}

SPH::~SPH() {}

void SPH::update(float dt)
{
    m_timeDelta = dt;
    m_prevSection = m_currentSection;
    m_currentSection = (m_currentSection + 1) % RendererConfig::VirtualFrameCount;

    /*if (!m_runSimulation)
        return;



    m_renderGraph->getNode("clear-hash-grid").isEnabled = true;
    m_renderGraph->getNode("compute-cell-count").isEnabled = true;
    m_renderGraph->getNode("scan").isEnabled = true;
    m_renderGraph->getNode("scan-block").isEnabled = true;
    m_renderGraph->getNode("scan-combine").isEnabled = true;
    m_renderGraph->getNode("reindex").isEnabled = true;
    m_renderGraph->getNode("compute-density-and-pressure").isEnabled = true;
    m_renderGraph->getNode("compute-forces").isEnabled = true;
    m_renderGraph->getNode("integrate").isEnabled = true;*/
}

void SPH::onKeyPressed(Key key, int)
{
    if (key == Key::Space)
        m_runSimulation = !m_runSimulation;
}

void SPH::setGravityX(float value)
{
    m_gravity.x = value;
}

void SPH::setGravityY(float value)
{
    m_gravity.y = value;
}

void SPH::setGravityZ(float value)
{
    m_gravity.z = value;
}

void SPH::setViscosity(float value)
{
    m_viscosityFactor = value;
}

void SPH::setSurfaceTension(float value)
{
    m_kappa = value;
}

void SPH::reset()
{
    m_runSimulation = false;
    m_renderer->finish();

    const std::size_t vertexBufferSize = m_numParticles * sizeof(glm::vec4);
    const auto positions = createInitialPositions(m_fluidDim, m_particleRadius);
    m_renderer->fillDeviceBuffer(m_vertexBuffer->getDeviceBuffer(), positions.data(), vertexBufferSize, 0);
    m_renderer->fillDeviceBuffer(
        m_vertexBuffer->getDeviceBuffer(), positions.data(), vertexBufferSize, vertexBufferSize);

    const auto velocities = std::vector<glm::vec4>(m_numParticles, glm::vec4(glm::vec3(0.0f), 1.0f));
    m_renderer->fillDeviceBuffer(m_velocityBuffer->getDeviceBuffer(), positions.data(), vertexBufferSize, 0);
    m_renderer->fillDeviceBuffer(
        m_velocityBuffer->getDeviceBuffer(), positions.data(), vertexBufferSize, vertexBufferSize);

    m_currentSection = 0;
    m_prevSection = 0;
}

float SPH::getParticleRadius() const
{
    return m_particleRadius;
}

VulkanBuffer* SPH::getVertexBuffer(std::string_view key) const
{
    if (key == "position")
        return m_vertexBuffer->getDeviceBuffer();
    else if (key == "color")
        return m_colorBuffer->getDeviceBuffer();
    else
        return nullptr;
}

uint32_t SPH::getParticleCount() const
{
    return m_numParticles;
}

uint32_t SPH::getCurrentSection() const
{
    return m_currentSection;
}

std::vector<glm::vec4> SPH::createInitialPositions(const glm::uvec3 fluidDim, const float particleRadius) const
{
    std::vector<glm::vec4> positions;
    positions.reserve(fluidDim.z * fluidDim.y * fluidDim.x);
    for (uint32_t z = 0; z < fluidDim.z; ++z)
        for (uint32_t y = 0; y < fluidDim.y; ++y)
            for (uint32_t x = 0; x < fluidDim.x; ++x)
            {
                // glm::vec3 translation = glm::vec3(m_fluidDim) * m_particleRadius;
                // translation.x = 0.0f;
                // translation.y = 0.0f;
                const glm::vec3 pos = glm::vec3(x, y, z) * 2.0f * particleRadius + particleRadius; // +translation;
                positions.emplace_back(pos, 1.0f);
            }
    return positions;
}
} // namespace crisp