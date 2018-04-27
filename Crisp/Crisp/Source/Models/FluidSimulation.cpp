#include "FluidSimulation.hpp"

#include "Renderer/VulkanRenderer.hpp"

#include "Renderer/Pipelines/ComputePipeline.hpp"


#include "glfw/glfw3.h"

namespace crisp
{
    namespace
    {
        static constexpr uint32_t ScanBlockSize = 256;
        static constexpr uint32_t ScanElementsPerThread = 2;
        static constexpr uint32_t ScanElementsPerBlock = ScanBlockSize * ScanElementsPerThread;
    }

    FluidSimulation::FluidSimulation(VulkanRenderer* renderer)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
        , m_particleRadius(0.01f)
        , m_currentSection(0)
        , m_prevSection(0)
        , m_runCompute(false)
        , m_timeDelta(0.0f)
    {
        m_fluidDim = glm::uvec3(64, 64, 32);
        m_numParticles = m_fluidDim.x * m_fluidDim.y * m_fluidDim.z;

        std::vector<glm::vec4> positions = createInitialPositions(m_fluidDim, m_particleRadius);

        std::size_t vertexBufferSize = m_numParticles * sizeof(glm::vec4);
        m_vertexBuffer = std::make_unique<VulkanBuffer>(m_device, VulkanRenderer::NumVirtualFrames * vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_renderer->fillDeviceBuffer(m_vertexBuffer.get(), positions.data(), vertexBufferSize, 0);
        m_renderer->fillDeviceBuffer(m_vertexBuffer.get(), positions.data(), vertexBufferSize, vertexBufferSize);
        m_renderer->fillDeviceBuffer(m_vertexBuffer.get(), positions.data(), vertexBufferSize, 2 * vertexBufferSize);

        auto colors = std::vector<glm::vec4>(m_numParticles, glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));
        m_colorBuffer = std::make_unique<VulkanBuffer>(m_device, VulkanRenderer::NumVirtualFrames * vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_renderer->fillDeviceBuffer(m_colorBuffer.get(), colors.data(), vertexBufferSize, 0);
        m_renderer->fillDeviceBuffer(m_colorBuffer.get(), colors.data(), vertexBufferSize, vertexBufferSize);
        m_renderer->fillDeviceBuffer(m_colorBuffer.get(), colors.data(), vertexBufferSize, 2 * vertexBufferSize);

        m_reorderedPositionBuffer = std::make_unique<VulkanBuffer>(m_device, VulkanRenderer::NumVirtualFrames * vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_fluidSpaceMin = glm::vec3(0.0f);
        //m_fluidSpaceMax = glm::vec3(m_fluidDim) * 2.0f * 2.0f * m_particleRadius;
        m_fluidSpaceMax = glm::vec3(m_fluidDim.x, m_fluidDim.y / 2, m_fluidDim.z / 2) * 4.0f * m_particleRadius;
        m_gridParams.cellSize = 4 * m_particleRadius;
        m_gridParams.spaceSize = m_fluidSpaceMax - m_fluidSpaceMin;
        m_gridParams.dim = glm::ivec3(glm::ceil((m_fluidSpaceMax - m_fluidSpaceMin) / m_gridParams.cellSize));
        m_gridParams.numCells = m_gridParams.dim.x * m_gridParams.dim.y * m_gridParams.dim.z;

        m_cellCountBuffer = std::make_unique<VulkanBuffer>(m_device, VulkanRenderer::NumVirtualFrames * m_gridParams.numCells * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_cellIdBuffer = std::make_unique<VulkanBuffer>(m_device, VulkanRenderer::NumVirtualFrames * m_numParticles * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_indexBuffer = std::make_unique<VulkanBuffer>(m_device, VulkanRenderer::NumVirtualFrames * m_numParticles * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_blockSumRegionSize = static_cast<uint32_t>(std::max(m_gridParams.numCells / ScanElementsPerBlock * sizeof(uint32_t), 32ull));
        m_blockSumBuffer = std::make_unique<VulkanBuffer>(m_device, VulkanRenderer::NumVirtualFrames * m_blockSumRegionSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_densityBuffer = std::make_unique<VulkanBuffer>(m_device, VulkanRenderer::NumVirtualFrames * m_numParticles * sizeof(float),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_pressureBuffer = std::make_unique<VulkanBuffer>(m_device, VulkanRenderer::NumVirtualFrames * m_numParticles * sizeof(float),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_velocityBuffer = std::make_unique<VulkanBuffer>(m_device, VulkanRenderer::NumVirtualFrames * m_numParticles * sizeof(glm::vec4),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        auto velocities = std::vector<glm::vec4>(m_numParticles, glm::vec4(glm::vec3(0.0f), 1.0f));
        m_renderer->fillDeviceBuffer(m_velocityBuffer.get(), velocities.data(), vertexBufferSize, 0);
        m_renderer->fillDeviceBuffer(m_velocityBuffer.get(), velocities.data(), vertexBufferSize, vertexBufferSize);
        m_renderer->fillDeviceBuffer(m_velocityBuffer.get(), velocities.data(), vertexBufferSize, 2 * vertexBufferSize);
        m_forcesBuffer = std::make_unique<VulkanBuffer>(m_device, VulkanRenderer::NumVirtualFrames * m_numParticles * sizeof(glm::vec4),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_clearHashGridPipeline = std::make_unique<ComputePipeline>(m_renderer, "clear-hash-grid-comp", 1, 1, sizeof(uint32_t), glm::uvec3(256, 1, 1));
        m_clearGridDescGroup =
        {
            m_clearHashGridPipeline->allocateDescriptorSet(0)
        };
        m_clearGridDescGroup.postBufferUpdate(0, 0, { m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        m_clearGridDescGroup.flushUpdates(m_device);

        m_computeCellCountPipeline = std::make_unique<ComputePipeline>(m_renderer, "compute-cell-count-comp", 3, 1, sizeof(glm::uvec3) + sizeof(float) + sizeof(uint32_t), glm::uvec3(256, 1, 1));
        m_computeCellCountDescGroup =
        {
            m_computeCellCountPipeline->allocateDescriptorSet(0)
        };
        m_computeCellCountDescGroup.postBufferUpdate(0, 0, { m_vertexBuffer->getHandle(),    0, vertexBufferSize });
        m_computeCellCountDescGroup.postBufferUpdate(0, 1, { m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        m_computeCellCountDescGroup.postBufferUpdate(0, 2, { m_cellIdBuffer->getHandle(),    0, m_numParticles * sizeof(uint32_t) });
        m_computeCellCountDescGroup.flushUpdates(m_device);

        // Scan for individual blocks
        m_scanPipeline = std::make_unique<ComputePipeline>(m_renderer, "scan-comp", 2, 2, sizeof(int32_t) + sizeof(uint32_t), glm::uvec3(256, 1, 1));
        m_scanDescGroup =
        {
            m_scanPipeline->allocateDescriptorSet(0)
        };
        m_scanDescGroup.postBufferUpdate(0, 0, { m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        m_scanDescGroup.postBufferUpdate(0, 1, { m_blockSumBuffer->getHandle(),  0, m_blockSumRegionSize });
        m_scanDescGroup.flushUpdates(m_device);

        // Scan for the block sums
        m_scanBlockDescGroup =
        {
            m_scanPipeline->allocateDescriptorSet(0)
        };
        m_scanBlockDescGroup.postBufferUpdate(0, 0, { m_blockSumBuffer->getHandle(), 0, m_blockSumRegionSize });
        m_scanBlockDescGroup.postBufferUpdate(0, 1, { m_blockSumBuffer->getHandle(), 0, m_blockSumRegionSize });
        m_scanBlockDescGroup.flushUpdates(m_device);

        // Add block prefix sum to intra-block prefix sums
        m_combineScanPipeline = std::make_unique<ComputePipeline>(m_renderer, "scan-combine-comp", 2, 1, sizeof(uint32_t), glm::uvec3(256, 1, 1));
        m_combineScanDescGroup =
        {
            m_combineScanPipeline->allocateDescriptorSet(0)
        };
        m_combineScanDescGroup.postBufferUpdate(0, 0, { m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        m_combineScanDescGroup.postBufferUpdate(0, 1, { m_blockSumBuffer->getHandle(),  0, m_blockSumRegionSize });
        m_combineScanDescGroup.flushUpdates(m_device);

        m_reindexPipeline = std::make_unique<ComputePipeline>(m_renderer, "reindex-particles-comp", 5, 1, sizeof(GridParams) + sizeof(float), glm::uvec3(256, 1, 1));
        m_reindexDescGroup =
        {
            m_reindexPipeline->allocateDescriptorSet(0)
        };
        m_reindexDescGroup.postBufferUpdate(0, 0, { m_vertexBuffer->getHandle(),              0, vertexBufferSize });
        m_reindexDescGroup.postBufferUpdate(0, 1, { m_cellCountBuffer->getHandle(),           0, m_gridParams.numCells * sizeof(uint32_t) });
        m_reindexDescGroup.postBufferUpdate(0, 2, { m_cellIdBuffer->getHandle(),              0, m_numParticles * sizeof(uint32_t) });
        m_reindexDescGroup.postBufferUpdate(0, 3, { m_indexBuffer->getHandle(),               0, m_numParticles * sizeof(uint32_t) });
        m_reindexDescGroup.postBufferUpdate(0, 4, { m_reorderedPositionBuffer->getHandle(),   0, m_numParticles * sizeof(glm::vec4) });
        m_reindexDescGroup.flushUpdates(m_device);

        m_densityPressurePipeline = std::make_unique<ComputePipeline>(m_renderer, "compute-density-and-pressure-comp", 5, 1, sizeof(GridParams) + sizeof(float), glm::uvec3(256, 1, 1));
        m_densityPressureDescGroup =
        {
            m_densityPressurePipeline->allocateDescriptorSet(0)
        };
        m_densityPressureDescGroup.postBufferUpdate(0, 0, { m_vertexBuffer->getHandle(),              0, vertexBufferSize });
        m_densityPressureDescGroup.postBufferUpdate(0, 1, { m_cellCountBuffer->getHandle(),           0, m_gridParams.numCells * sizeof(uint32_t) });
        m_densityPressureDescGroup.postBufferUpdate(0, 2, { m_reorderedPositionBuffer->getHandle(),   0, vertexBufferSize });
        //m_densityPressureDescGroup.postBufferUpdate(0, 2, { m_indexBuffer->getHandle(),   0, m_numParticles * sizeof(uint32_t) });
        m_densityPressureDescGroup.postBufferUpdate(0, 3, { m_densityBuffer->getHandle(),             0, m_numParticles * sizeof(float) });
        m_densityPressureDescGroup.postBufferUpdate(0, 4, { m_pressureBuffer->getHandle(),            0, m_numParticles * sizeof(float) });
        m_densityPressureDescGroup.flushUpdates(m_device);

        m_forcesPipeline = std::make_unique<ComputePipeline>(m_renderer, "compute-forces-comp", 7, 1, sizeof(GridParams) + sizeof(glm::uvec3) + sizeof(uint32_t) + 2 * sizeof(float), glm::uvec3(256, 1, 1));
        m_forcesDescGroup =
        {
            m_forcesPipeline->allocateDescriptorSet(0)
        };
        m_forcesDescGroup.postBufferUpdate(0, 0, { m_vertexBuffer->getHandle(),    0, vertexBufferSize });
        m_forcesDescGroup.postBufferUpdate(0, 1, { m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        m_forcesDescGroup.postBufferUpdate(0, 2, { m_indexBuffer->getHandle(),     0, m_numParticles * sizeof(uint32_t) });
        m_forcesDescGroup.postBufferUpdate(0, 3, { m_densityBuffer->getHandle(),   0, m_numParticles * sizeof(float) });
        m_forcesDescGroup.postBufferUpdate(0, 4, { m_pressureBuffer->getHandle(),  0, m_numParticles * sizeof(float) });
        m_forcesDescGroup.postBufferUpdate(0, 5, { m_velocityBuffer->getHandle(),  0, m_numParticles * sizeof(glm::vec4) });
        m_forcesDescGroup.postBufferUpdate(0, 6, { m_forcesBuffer->getHandle(),    0, m_numParticles * sizeof(glm::vec4) });
        m_forcesDescGroup.flushUpdates(m_device);

        m_integratePipeline = std::make_unique<ComputePipeline>(m_renderer, "integrate-comp", 6, 1, sizeof(GridParams) + sizeof(uint32_t) + sizeof(float), glm::uvec3(256, 1, 1));
        m_integrateDescGroup =
        {
            m_integratePipeline->allocateDescriptorSet(0)
        };
        m_integrateDescGroup.postBufferUpdate(0, 0, { m_vertexBuffer->getHandle(),    0, m_numParticles * sizeof(glm::vec4) });
        m_integrateDescGroup.postBufferUpdate(0, 1, { m_velocityBuffer->getHandle(),  0, m_numParticles * sizeof(glm::vec4) });
        m_integrateDescGroup.postBufferUpdate(0, 2, { m_forcesBuffer->getHandle(),    0, m_numParticles * sizeof(glm::vec4) });
        m_integrateDescGroup.postBufferUpdate(0, 3, { m_vertexBuffer->getHandle(),    0, m_numParticles * sizeof(glm::vec4) });
        m_integrateDescGroup.postBufferUpdate(0, 4, { m_velocityBuffer->getHandle(),  0, m_numParticles * sizeof(glm::vec4) });
        m_integrateDescGroup.postBufferUpdate(0, 5, { m_colorBuffer->getHandle(),     0, m_numParticles * sizeof(glm::vec4) });
        m_integrateDescGroup.flushUpdates(m_device);
    }

    FluidSimulation::~FluidSimulation()
    {
    }

    void FluidSimulation::update(float dt)
    {
        m_timeDelta = dt;
        m_runCompute = true;
    }

    void FluidSimulation::dispatchCompute(VkCommandBuffer cmdBuffer, uint32_t currentFrameIdx) const
    {
        if (!m_runSimulation)
            return;

        if (m_runCompute)
        {
            m_runCompute = false;

            m_prevSection = m_currentSection;
            m_currentSection = currentFrameIdx;
            for (int i = 0; i < 5; i++)
            {
                clearCellCounts(cmdBuffer);
                computeCellCounts(cmdBuffer);
                scanCellCounts(cmdBuffer);
                reindex(cmdBuffer);
                computeDensityAndPressure(cmdBuffer);
                computeForces(cmdBuffer);
                integrate(cmdBuffer, m_timeDelta / 5.0f);
                insertComputeBarrier(cmdBuffer);
                m_prevSection = m_currentSection;
                m_currentSection = (m_currentSection + 1) % 3;
            }

            VkMemoryBarrier memBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
            memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);
        }
    }

    void FluidSimulation::onKeyPressed(int code, int)
    {
        if (code == GLFW_KEY_SPACE)
            m_runSimulation = !m_runSimulation;
    }

    void FluidSimulation::setGravityX(float value)
    {
        m_gravity.x = value;
    }

    void FluidSimulation::setGravityY(float value)
    {
        m_gravity.y = value;
    }

    void FluidSimulation::setGravityZ(float value)
    {
        m_gravity.z = value;
    }

    void FluidSimulation::setViscosity(float value)
    {
        m_viscosityFactor = value;
    }

    void FluidSimulation::setSurfaceTension(float value)
    {
        m_kappa = value;
    }

    void FluidSimulation::reset()
    {
        m_runSimulation = false;
        m_renderer->finish();

        std::vector<glm::vec4> positions = createInitialPositions(m_fluidDim, m_particleRadius);

        std::size_t vertexBufferSize = m_numParticles * sizeof(glm::vec4);
        m_renderer->fillDeviceBuffer(m_vertexBuffer.get(), positions.data(), vertexBufferSize, 0);
        m_renderer->fillDeviceBuffer(m_vertexBuffer.get(), positions.data(), vertexBufferSize, vertexBufferSize);
        m_renderer->fillDeviceBuffer(m_vertexBuffer.get(), positions.data(), vertexBufferSize, 2 * vertexBufferSize);

        auto velocities = std::vector<glm::vec4>(m_numParticles, glm::vec4(glm::vec3(0.0f), 1.0f));
        m_renderer->fillDeviceBuffer(m_velocityBuffer.get(), velocities.data(), vertexBufferSize, 0);
        m_renderer->fillDeviceBuffer(m_velocityBuffer.get(), velocities.data(), vertexBufferSize, vertexBufferSize);
        m_renderer->fillDeviceBuffer(m_velocityBuffer.get(), velocities.data(), vertexBufferSize, 2 * vertexBufferSize);

        m_currentSection = 0;
        m_prevSection = 0;
    }

    float FluidSimulation::getParticleRadius() const
    {
        return m_particleRadius;
    }

    void FluidSimulation::drawGeometry(VkCommandBuffer cmdBuffer) const
    {
        VkDeviceSize offset = m_currentSection * m_numParticles * sizeof(glm::vec4);
        VkDeviceSize offsets[] = { offset, offset };
        VkBuffer buffers[] = { m_vertexBuffer->getHandle(), m_colorBuffer->getHandle() };
        vkCmdBindVertexBuffers(cmdBuffer, 0, 2, buffers, offsets);
        vkCmdDraw(cmdBuffer, m_numParticles, 1, 0, 0);
    }

    void FluidSimulation::clearCellCounts(VkCommandBuffer cmdBuffer) const
    {
        m_clearHashGridPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_clearHashGridPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_gridParams.numCells);
        m_clearGridDescGroup.setDynamicOffset(0, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_clearGridDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_clearHashGridPipeline->getPipelineLayout());

        glm::uvec3 numGroups = (glm::uvec3(m_gridParams.numCells, 1, 1) - glm::uvec3(1)) / m_clearHashGridPipeline->getWorkGroupSize() + glm::uvec3(1);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);
    }

    void FluidSimulation::computeCellCounts(VkCommandBuffer cmdBuffer) const
    {
        m_computeCellCountPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_computeCellCountPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_gridParams.dim);
        m_computeCellCountPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(m_gridParams.dim), m_gridParams.cellSize);
        m_computeCellCountPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(m_gridParams.dim) + sizeof(m_gridParams.cellSize), m_numParticles);
        m_computeCellCountDescGroup.setDynamicOffset(0, m_prevSection * m_numParticles * sizeof(glm::vec4));
        m_computeCellCountDescGroup.setDynamicOffset(1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_computeCellCountDescGroup.setDynamicOffset(2, m_currentSection * m_numParticles * sizeof(uint32_t));
        m_computeCellCountDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_computeCellCountPipeline->getPipelineLayout());

        glm::uvec3 numGroups = (glm::uvec3(m_numParticles, 1, 1) - glm::uvec3(1)) / m_computeCellCountPipeline->getWorkGroupSize() + glm::uvec3(1);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);
    }

    void FluidSimulation::scanCellCounts(VkCommandBuffer cmdBuffer) const
    {
        // Intra-block scan
        m_scanPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_scanPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, 1);
        m_scanPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 4, m_gridParams.numCells);
        m_scanDescGroup.setDynamicOffset(0, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_scanDescGroup.setDynamicOffset(1, m_currentSection * m_blockSumRegionSize);
        m_scanDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_scanPipeline->getPipelineLayout());

        glm::uvec3 numBlocks = glm::uvec3(m_gridParams.numCells, 1, 1) / glm::uvec3(ScanElementsPerBlock, 1, 1);
        vkCmdDispatch(cmdBuffer, numBlocks.x, numBlocks.y, numBlocks.z);
        insertComputeBarrier(cmdBuffer);

        // Block-level scan
        m_scanPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, 0);
        m_scanPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 4, m_gridParams.numCells / ScanElementsPerBlock);
        m_scanBlockDescGroup.setDynamicOffset(0, m_currentSection * m_blockSumRegionSize);
        m_scanBlockDescGroup.setDynamicOffset(1, m_currentSection * m_blockSumRegionSize);
        m_scanBlockDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_scanPipeline->getPipelineLayout());

        numBlocks = (glm::uvec3(m_gridParams.numCells / ScanElementsPerBlock, 1, 1) - glm::uvec3(1))
            / glm::uvec3(ScanElementsPerBlock, 1, 1) + glm::uvec3(1);
        vkCmdDispatch(cmdBuffer, numBlocks.x, numBlocks.y, numBlocks.z);
        insertComputeBarrier(cmdBuffer);

        // Fold the block-level scans into intra-block scans
        m_combineScanPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_combineScanPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_gridParams.numCells);
        m_combineScanDescGroup.setDynamicOffset(0, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_combineScanDescGroup.setDynamicOffset(1, m_currentSection * m_blockSumRegionSize);
        m_combineScanDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_combineScanPipeline->getPipelineLayout());

        numBlocks = glm::uvec3(m_gridParams.numCells, 1, 1) / glm::uvec3(ScanElementsPerBlock, 1, 1);
        vkCmdDispatch(cmdBuffer, numBlocks.x, numBlocks.y, numBlocks.z);
        insertComputeBarrier(cmdBuffer);
    }

    void FluidSimulation::reindex(VkCommandBuffer cmdBuffer) const
    {
        m_reindexPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_reindexPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_gridParams);
        m_reindexPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(m_gridParams), m_numParticles);
        m_reindexDescGroup.setDynamicOffset(0, m_prevSection * m_numParticles * sizeof(glm::vec4));
        m_reindexDescGroup.setDynamicOffset(1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_reindexDescGroup.setDynamicOffset(2, m_currentSection * m_numParticles * sizeof(uint32_t));
        m_reindexDescGroup.setDynamicOffset(3, m_currentSection * m_numParticles * sizeof(uint32_t));
        m_reindexDescGroup.setDynamicOffset(4, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_reindexDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_reindexPipeline->getPipelineLayout());

        glm::uvec3 numGroups = (glm::uvec3(m_numParticles, 1, 1) - glm::uvec3(1)) / m_reindexPipeline->getWorkGroupSize() + glm::uvec3(1);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);
    }

    void FluidSimulation::computeDensityAndPressure(VkCommandBuffer cmdBuffer) const
    {
        m_densityPressurePipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_densityPressurePipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_gridParams);
        m_densityPressurePipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(m_gridParams), m_numParticles);
        m_densityPressureDescGroup.setDynamicOffset(0, m_prevSection * m_numParticles * sizeof(glm::vec4));
        m_densityPressureDescGroup.setDynamicOffset(1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_densityPressureDescGroup.setDynamicOffset(2, m_currentSection * m_numParticles * sizeof(glm::vec4));
        //m_densityPressureDescGroup.setDynamicOffset(2, m_currentSection * m_numParticles * sizeof(uint32_t));
        m_densityPressureDescGroup.setDynamicOffset(3, m_currentSection * m_numParticles * sizeof(float));
        m_densityPressureDescGroup.setDynamicOffset(4, m_currentSection * m_numParticles * sizeof(float));
        m_densityPressureDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_densityPressurePipeline->getPipelineLayout());

        glm::uvec3 numGroups = (glm::uvec3(m_numParticles, 1, 1) - glm::uvec3(1)) / m_densityPressurePipeline->getWorkGroupSize() + glm::uvec3(1);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);
    }

    void FluidSimulation::computeForces(VkCommandBuffer cmdBuffer) const
    {
        m_forcesPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_forcesPipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, m_gravity, m_numParticles, m_viscosityFactor, m_kappa);
        m_forcesDescGroup.setDynamicOffset(0, m_prevSection * m_numParticles * sizeof(glm::vec4));
        m_forcesDescGroup.setDynamicOffset(1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_forcesDescGroup.setDynamicOffset(2, m_currentSection * m_numParticles * sizeof(uint32_t));
        m_forcesDescGroup.setDynamicOffset(3, m_currentSection * m_numParticles * sizeof(float));
        m_forcesDescGroup.setDynamicOffset(4, m_currentSection * m_numParticles * sizeof(float));
        m_forcesDescGroup.setDynamicOffset(5, m_prevSection * m_numParticles * sizeof(glm::vec4));
        m_forcesDescGroup.setDynamicOffset(6, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_forcesDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_forcesPipeline->getPipelineLayout());

        glm::uvec3 numGroups = (glm::uvec3(m_numParticles, 1, 1) - glm::uvec3(1)) / m_forcesPipeline->getWorkGroupSize() + glm::uvec3(1);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);
    }

    void FluidSimulation::integrate(VkCommandBuffer cmdBuffer, float timeDelta) const
    {
        m_integratePipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_integratePipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, timeDelta, m_numParticles);
        m_integrateDescGroup.setDynamicOffset(0, m_prevSection * m_numParticles * sizeof(glm::vec4));
        m_integrateDescGroup.setDynamicOffset(1, m_prevSection * m_numParticles * sizeof(glm::vec4));
        m_integrateDescGroup.setDynamicOffset(2, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_integrateDescGroup.setDynamicOffset(3, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_integrateDescGroup.setDynamicOffset(4, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_integrateDescGroup.setDynamicOffset(5, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_integrateDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_integratePipeline->getPipelineLayout());

        glm::uvec3 numGroups = (glm::uvec3(m_numParticles, 1, 1) - glm::uvec3(1)) / m_integratePipeline->getWorkGroupSize() + glm::uvec3(1);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
    }

    void FluidSimulation::insertComputeBarrier(VkCommandBuffer cmdBuffer) const
    {
        VkMemoryBarrier memBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);
    }

    std::vector<glm::vec4> FluidSimulation::createInitialPositions(glm::uvec3 fluidDim, float particleRadius) const
    {
        std::vector<glm::vec4> positions;
        for (unsigned int z = 0; z < fluidDim.z; ++z)
            for (unsigned int y = 0; y < fluidDim.y; ++y)
                for (unsigned int x = 0; x < fluidDim.x; ++x)
                {
                    //glm::vec3 translation = glm::vec3(m_fluidDim) * m_particleRadius;
                    //translation.x = 0.0f;
                    //translation.y = 0.0f;
                    glm::vec3 offset = glm::vec3(x, y, z) * 2.0f * particleRadius + particleRadius;// +translation;
                    positions.emplace_back(offset, 1.0f);
                }
        return positions;
    }
}