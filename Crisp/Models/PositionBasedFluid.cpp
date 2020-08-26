#include "PositionBasedFluid.hpp"

#include "Renderer/Renderer.hpp"

#include "Renderer/Pipelines/ComputePipeline.hpp"


#include "glfw/glfw3.h"

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
    }

    PositionBasedFluid::PositionBasedFluid(Renderer* renderer)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
        , m_particleRadius(0.01f)
        , m_currentSection(0)
        , m_prevSection(0)
        , m_runCompute(false)
        , m_timeDelta(0.0f)
    {
        m_fluidDim = glm::uvec3(32, 32, 32);
        m_numParticles = m_fluidDim.x * m_fluidDim.y * m_fluidDim.z;

        std::vector<glm::vec4> positions = createInitialPositions(m_fluidDim, m_particleRadius);

        std::size_t vertexBufferSize = m_numParticles * sizeof(glm::vec4);
        m_positions = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_renderer->fillDeviceBuffer(m_positions.get(), positions.data(), vertexBufferSize, 0);

        auto colors = std::vector<glm::vec4>(m_numParticles, glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));
        m_colors = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_renderer->fillDeviceBuffer(m_colors.get(), colors.data(), vertexBufferSize, 0);

        m_reorderedPositions = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * vertexBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_fluidSpaceMin = glm::vec3(0.0f);
        m_fluidSpaceMax = glm::vec3(m_fluidDim.x, m_fluidDim.y / 2, m_fluidDim.z / 2) * 4.0f * m_particleRadius;
        m_gridParams.cellSize = 4 * m_particleRadius;
        m_gridParams.spaceSize = m_fluidSpaceMax - m_fluidSpaceMin;
        m_gridParams.dim = glm::ivec3(glm::ceil((m_fluidSpaceMax - m_fluidSpaceMin) / m_gridParams.cellSize));
        m_gridParams.numCells = m_gridParams.dim.x * m_gridParams.dim.y * m_gridParams.dim.z;

        m_cellCounts = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_gridParams.numCells * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_cellIndices = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_indices = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_blockSumRegionSize = static_cast<uint32_t>(std::max(m_gridParams.numCells / ScanElementsPerBlock * sizeof(uint32_t), 32ull));
        m_blockSums = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_blockSumRegionSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_lambdas = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(float),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_deltas = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(glm::vec4),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_pressures = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(float),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_velocities = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(glm::vec4),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        auto velocities = std::vector<glm::vec4>(m_numParticles, glm::vec4(glm::vec3(0.0f), 1.0f));
        m_renderer->fillDeviceBuffer(m_velocities.get(), velocities.data(), vertexBufferSize, 0);

        auto forces = std::vector<glm::vec4>(m_numParticles, glm::vec4(0.0f, 0.0f, 0.0f, 1000.0f));
        m_forces = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(glm::vec4),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_renderer->fillDeviceBuffer(m_forces.get(), forces.data(), vertexBufferSize, 0);

        m_clearHashGridPipeline = createComputePipeline(m_renderer, "clear-hash-grid-comp", 1, 1, sizeof(uint32_t), glm::uvec3(256, 1, 1));
        m_clearGridDescGroup =
        {
            m_clearHashGridPipeline->allocateDescriptorSet(0)
        };
        m_clearGridDescGroup.postBufferUpdate(0, 0, { m_cellCounts->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        m_clearGridDescGroup.flushUpdates(m_device);

        m_computeCellCountPipeline = createComputePipeline(m_renderer, "compute-cell-count-comp", 3, 1, sizeof(glm::uvec3) + sizeof(float) + sizeof(uint32_t), glm::uvec3(256, 1, 1));
        m_computeCellCountDescGroup =
        {
            m_computeCellCountPipeline->allocateDescriptorSet(0)
        };
        m_computeCellCountDescGroup.postBufferUpdate(0, 0, { m_positions->getHandle(),    0, vertexBufferSize });
        m_computeCellCountDescGroup.postBufferUpdate(0, 1, { m_cellCounts->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        m_computeCellCountDescGroup.postBufferUpdate(0, 2, { m_cellIndices->getHandle(),    0, m_numParticles * sizeof(uint32_t) });
        m_computeCellCountDescGroup.flushUpdates(m_device);

        // Scan for individual blocks
        m_scanPipeline = createComputePipeline(m_renderer, "scan-comp", 2, 2, sizeof(int32_t) + sizeof(uint32_t), glm::uvec3(256, 1, 1));
        m_scanDescGroup =
        {
            m_scanPipeline->allocateDescriptorSet(0)
        };
        m_scanDescGroup.postBufferUpdate(0, 0, { m_cellCounts->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        m_scanDescGroup.postBufferUpdate(0, 1, { m_blockSums->getHandle(),  0, m_blockSumRegionSize });
        m_scanDescGroup.flushUpdates(m_device);

        // Scan for the block sums
        m_scanBlockDescGroup =
        {
            m_scanPipeline->allocateDescriptorSet(0)
        };
        m_scanBlockDescGroup.postBufferUpdate(0, 0, { m_blockSums->getHandle(), 0, m_blockSumRegionSize });
        m_scanBlockDescGroup.postBufferUpdate(0, 1, { m_blockSums->getHandle(), 0, m_blockSumRegionSize });
        m_scanBlockDescGroup.flushUpdates(m_device);

        // Add block prefix sum to intra-block prefix sums
        m_combineScanPipeline = createComputePipeline(m_renderer, "scan-combine-comp", 2, 1, sizeof(uint32_t), glm::uvec3(512, 1, 1));
        m_combineScanDescGroup =
        {
            m_combineScanPipeline->allocateDescriptorSet(0)
        };
        m_combineScanDescGroup.postBufferUpdate(0, 0, { m_cellCounts->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        m_combineScanDescGroup.postBufferUpdate(0, 1, { m_blockSums->getHandle(),  0, m_blockSumRegionSize });
        m_combineScanDescGroup.flushUpdates(m_device);

        m_reindexPipeline = createComputePipeline(m_renderer, "reindex-particles-comp", 5, 1, sizeof(GridParams) + sizeof(float), glm::uvec3(256, 1, 1));
        m_reindexDescGroup =
        {
            m_reindexPipeline->allocateDescriptorSet(0)
        };
        m_reindexDescGroup.postBufferUpdate(0, 0, { m_positions->getHandle(),          0, m_numParticles * sizeof(glm::vec4) });
        m_reindexDescGroup.postBufferUpdate(0, 1, { m_cellCounts->getHandle(),         0, m_gridParams.numCells * sizeof(uint32_t) });
        m_reindexDescGroup.postBufferUpdate(0, 2, { m_cellIndices->getHandle(),        0, m_numParticles * sizeof(uint32_t) });
        m_reindexDescGroup.postBufferUpdate(0, 3, { m_indices->getHandle(),            0, m_numParticles * sizeof(uint32_t) });
        m_reindexDescGroup.postBufferUpdate(0, 4, { m_reorderedPositions->getHandle(), 0, m_numParticles * sizeof(glm::vec4) });
        m_reindexDescGroup.flushUpdates(m_device);

        m_lambdasPipeline = createComputePipeline(m_renderer, "pbf-compute-lambdas-comp", 4, 1, sizeof(GridParams) + sizeof(float), glm::uvec3(256, 1, 1));
        m_lambdasDescGroup =
        {
            m_lambdasPipeline->allocateDescriptorSet(0)
        };
        m_lambdasDescGroup.postBufferUpdate(0, 0, { m_positions->getHandle(),  0, m_numParticles * sizeof(glm::vec4) });
        m_lambdasDescGroup.postBufferUpdate(0, 1, { m_cellCounts->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        m_lambdasDescGroup.postBufferUpdate(0, 2, { m_indices->getHandle(),    0, m_numParticles * sizeof(uint32_t) });
        m_lambdasDescGroup.postBufferUpdate(0, 3, { m_lambdas->getHandle(),    0, m_numParticles * sizeof(float) });
        m_lambdasDescGroup.flushUpdates(m_device);

        m_deltasPipeline = createComputePipeline(m_renderer, "pbf-compute-deltas-comp", 5, 1, sizeof(GridParams) + sizeof(float), glm::uvec3(256, 1, 1));
        m_deltasDescGroup =
        {
            m_deltasPipeline->allocateDescriptorSet(0)
        };
        m_deltasDescGroup.postBufferUpdate(0, 0, { m_positions->getHandle(),  0, m_numParticles * sizeof(glm::vec4) });
        m_deltasDescGroup.postBufferUpdate(0, 1, { m_cellCounts->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        m_deltasDescGroup.postBufferUpdate(0, 2, { m_indices->getHandle(),    0, m_numParticles * sizeof(uint32_t) });
        m_deltasDescGroup.postBufferUpdate(0, 3, { m_lambdas->getHandle(),    0, m_numParticles * sizeof(float) });
        m_deltasDescGroup.postBufferUpdate(0, 4, { m_deltas->getHandle(),     0, m_numParticles * sizeof(glm::vec4) });
        m_deltasDescGroup.flushUpdates(m_device);

        m_updatePosPipeline = createComputePipeline(m_renderer, "pbf-update-pos-comp", 2, 1, sizeof(uint32_t), glm::uvec3(256, 1, 1));
        m_updatePosDescGroup =
        {
            m_updatePosPipeline->allocateDescriptorSet(0)
        };
        m_updatePosDescGroup.postBufferUpdate(0, 0, { m_positions->getHandle(),  0, m_numParticles * sizeof(glm::vec4) });
        m_updatePosDescGroup.postBufferUpdate(0, 1, { m_deltas->getHandle(),     0, m_numParticles * sizeof(glm::vec4) });
        m_updatePosDescGroup.flushUpdates(m_device);

        m_forcesPipeline = createComputePipeline(m_renderer, "compute-forces-comp", 7, 1, sizeof(GridParams) + sizeof(glm::uvec3) + sizeof(uint32_t) + 2 * sizeof(float), glm::uvec3(256, 1, 1));
        m_forcesDescGroup =
        {
            m_forcesPipeline->allocateDescriptorSet(0)
        };
        m_forcesDescGroup.postBufferUpdate(0, 0, { m_positions->getHandle(),    0, vertexBufferSize });
        m_forcesDescGroup.postBufferUpdate(0, 1, { m_cellCounts->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        m_forcesDescGroup.postBufferUpdate(0, 2, { m_indices->getHandle(),     0, m_numParticles * sizeof(uint32_t) });
        m_forcesDescGroup.postBufferUpdate(0, 3, { m_lambdas->getHandle(),   0, m_numParticles * sizeof(float) });
        m_forcesDescGroup.postBufferUpdate(0, 4, { m_pressures->getHandle(),  0, m_numParticles * sizeof(float) });
        m_forcesDescGroup.postBufferUpdate(0, 5, { m_velocities->getHandle(),  0, m_numParticles * sizeof(glm::vec4) });
        m_forcesDescGroup.postBufferUpdate(0, 6, { m_forces->getHandle(),    0, m_numParticles * sizeof(glm::vec4) });
        m_forcesDescGroup.flushUpdates(m_device);

        m_predictPositionsPipeline = createComputePipeline(m_renderer, "pbf-predict-comp", 6, 1, sizeof(GridParams) + sizeof(uint32_t) + sizeof(float), glm::uvec3(256, 1, 1));
        m_predictPosDescGroup =
        {
            m_predictPositionsPipeline->allocateDescriptorSet(0)
        };
        m_predictPosDescGroup.postBufferUpdate(0, 0, { m_positions->getHandle(),    0, m_numParticles * sizeof(glm::vec4) });
        m_predictPosDescGroup.postBufferUpdate(0, 1, { m_velocities->getHandle(),  0, m_numParticles * sizeof(glm::vec4) });
        m_predictPosDescGroup.postBufferUpdate(0, 2, { m_forces->getHandle(),    0, m_numParticles * sizeof(glm::vec4) });
        m_predictPosDescGroup.postBufferUpdate(0, 3, { m_positions->getHandle(),    0, m_numParticles * sizeof(glm::vec4) });
        m_predictPosDescGroup.postBufferUpdate(0, 4, { m_velocities->getHandle(),  0, m_numParticles * sizeof(glm::vec4) });
        m_predictPosDescGroup.postBufferUpdate(0, 5, { m_colors->getHandle(),     0, m_numParticles * sizeof(glm::vec4) });
        m_predictPosDescGroup.flushUpdates(m_device);
    }

    PositionBasedFluid::~PositionBasedFluid()
    {
    }

    void PositionBasedFluid::update(float dt)
    {
        m_timeDelta = dt;
        m_runCompute = true;
    }

    void PositionBasedFluid::dispatchCompute(VkCommandBuffer cmdBuffer, uint32_t currentFrameIdx) const
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
                predictPositions(cmdBuffer, m_timeDelta / 5.0f);
                clearCellCounts(cmdBuffer);
                computeCellCounts(cmdBuffer);
                scanCellCounts(cmdBuffer);
                reindex(cmdBuffer);
                for (int j = 0; j < 3; j++)
                {
                    computeLambdas(cmdBuffer);
                    computeDeltas(cmdBuffer);
                    updatePositions(cmdBuffer);
                }
                updateAll(cmdBuffer, m_timeDelta);
                addVorticity(cmdBuffer);
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

    void PositionBasedFluid::onKeyPressed(Key key, int)
    {
        if (key == Key::Space)
            m_runSimulation = !m_runSimulation;
    }

    void PositionBasedFluid::setGravityX(float value)
    {
        m_gravity.x = value;
    }

    void PositionBasedFluid::setGravityY(float value)
    {
        m_gravity.y = value;
    }

    void PositionBasedFluid::setGravityZ(float value)
    {
        m_gravity.z = value;
    }

    void PositionBasedFluid::setViscosity(float value)
    {
        m_viscosityFactor = value;
    }

    void PositionBasedFluid::setSurfaceTension(float value)
    {
        m_kappa = value;
    }

    void PositionBasedFluid::reset()
    {
        m_runSimulation = false;
        m_renderer->finish();

        std::vector<glm::vec4> positions = createInitialPositions(m_fluidDim, m_particleRadius);

        std::size_t vertexBufferSize = m_numParticles * sizeof(glm::vec4);
        m_renderer->fillDeviceBuffer(m_positions.get(), positions.data(), vertexBufferSize, 0);
        m_renderer->fillDeviceBuffer(m_positions.get(), positions.data(), vertexBufferSize, vertexBufferSize);
        m_renderer->fillDeviceBuffer(m_positions.get(), positions.data(), vertexBufferSize, 2 * vertexBufferSize);

        auto velocities = std::vector<glm::vec4>(m_numParticles, glm::vec4(glm::vec3(0.0f), 1.0f));
        m_renderer->fillDeviceBuffer(m_velocities.get(), velocities.data(), vertexBufferSize, 0);
        m_renderer->fillDeviceBuffer(m_velocities.get(), velocities.data(), vertexBufferSize, vertexBufferSize);
        m_renderer->fillDeviceBuffer(m_velocities.get(), velocities.data(), vertexBufferSize, 2 * vertexBufferSize);

        m_currentSection = 0;
        m_prevSection = 0;
    }

    float PositionBasedFluid::getParticleRadius() const
    {
        return m_particleRadius;
    }

    void PositionBasedFluid::drawGeometry(VkCommandBuffer cmdBuffer) const
    {
        VkDeviceSize offset = m_currentSection * m_numParticles * sizeof(glm::vec4);
        VkDeviceSize offsets[] = { offset, offset };
        VkBuffer buffers[] = { m_positions->getHandle(), m_colors->getHandle() };
        vkCmdBindVertexBuffers(cmdBuffer, 0, 2, buffers, offsets);
        vkCmdDraw(cmdBuffer, m_numParticles, 1, 0, 0);
    }

    void PositionBasedFluid::predictPositions(VkCommandBuffer cmdBuffer, float timeDelta) const
    {
        m_predictPositionsPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_predictPositionsPipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, timeDelta, m_numParticles);
        m_predictPosDescGroup.setDynamicOffset(0, m_prevSection * m_numParticles * sizeof(glm::vec4));
        m_predictPosDescGroup.setDynamicOffset(1, m_prevSection * m_numParticles * sizeof(glm::vec4));
        m_predictPosDescGroup.setDynamicOffset(2, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_predictPosDescGroup.setDynamicOffset(3, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_predictPosDescGroup.setDynamicOffset(4, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_predictPosDescGroup.setDynamicOffset(5, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_predictPosDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_predictPositionsPipeline->getPipelineLayout()->getHandle());

        glm::uvec3 numGroups = getNumWorkGroups(glm::uvec3(m_numParticles, 1, 1), *m_predictPositionsPipeline);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
    }

    void PositionBasedFluid::clearCellCounts(VkCommandBuffer cmdBuffer) const
    {
        m_clearHashGridPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_clearHashGridPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_gridParams.numCells);
        m_clearGridDescGroup.setDynamicOffset(0, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_clearGridDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_clearHashGridPipeline->getPipelineLayout()->getHandle());

        glm::uvec3 numGroups = getNumWorkGroups(glm::uvec3(m_gridParams.numCells, 1, 1), *m_clearHashGridPipeline);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);
    }

    void PositionBasedFluid::computeCellCounts(VkCommandBuffer cmdBuffer) const
    {
        m_computeCellCountPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_computeCellCountPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_gridParams.dim);
        m_computeCellCountPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(m_gridParams.dim), m_gridParams.cellSize);
        m_computeCellCountPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(m_gridParams.dim) + sizeof(m_gridParams.cellSize), m_numParticles);
        m_computeCellCountDescGroup.setDynamicOffset(0, m_prevSection * m_numParticles * sizeof(glm::vec4));
        m_computeCellCountDescGroup.setDynamicOffset(1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_computeCellCountDescGroup.setDynamicOffset(2, m_currentSection * m_numParticles * sizeof(uint32_t));
        m_computeCellCountDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_computeCellCountPipeline->getPipelineLayout()->getHandle());

        glm::uvec3 numGroups = getNumWorkGroups(glm::uvec3(m_numParticles, 1, 1), *m_computeCellCountPipeline);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);
    }

    void PositionBasedFluid::scanCellCounts(VkCommandBuffer cmdBuffer) const
    {
        // Intra-block scan
        m_scanPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_scanPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, 1);
        m_scanPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 4, m_gridParams.numCells);
        m_scanDescGroup.setDynamicOffset(0, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_scanDescGroup.setDynamicOffset(1, m_currentSection * m_blockSumRegionSize);
        m_scanDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_scanPipeline->getPipelineLayout()->getHandle());

        glm::uvec3 numGroups = getNumWorkGroups(glm::uvec3(m_gridParams.numCells / ScanElementsPerThread, 1, 1), *m_scanPipeline);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);

        // Block-level scan
        m_scanPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, 0);
        m_scanPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 4, m_gridParams.numCells / ScanElementsPerBlock);
        m_scanBlockDescGroup.setDynamicOffset(0, m_currentSection * m_blockSumRegionSize);
        m_scanBlockDescGroup.setDynamicOffset(1, m_currentSection * m_blockSumRegionSize);
        m_scanBlockDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_scanPipeline->getPipelineLayout()->getHandle());

        numGroups = getNumWorkGroups(glm::uvec3(m_gridParams.numCells / ScanElementsPerBlock / ScanElementsPerThread, 1, 1), *m_scanPipeline);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);

        // Fold the block-level scans into intra-block scans
        m_combineScanPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_combineScanPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_gridParams.numCells);
        m_combineScanDescGroup.setDynamicOffset(0, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_combineScanDescGroup.setDynamicOffset(1, m_currentSection * m_blockSumRegionSize);
        m_combineScanDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_combineScanPipeline->getPipelineLayout()->getHandle());

        numGroups = getNumWorkGroups(glm::uvec3(m_gridParams.numCells, 1, 1), *m_combineScanPipeline);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);
    }

    void PositionBasedFluid::reindex(VkCommandBuffer cmdBuffer) const
    {
        m_reindexPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_reindexPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_gridParams);
        m_reindexPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(m_gridParams), m_numParticles);
        m_reindexDescGroup.setDynamicOffset(0, m_prevSection * m_numParticles * sizeof(glm::vec4));
        m_reindexDescGroup.setDynamicOffset(1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_reindexDescGroup.setDynamicOffset(2, m_currentSection * m_numParticles * sizeof(uint32_t));
        m_reindexDescGroup.setDynamicOffset(3, m_currentSection * m_numParticles * sizeof(uint32_t));
        m_reindexDescGroup.setDynamicOffset(4, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_reindexDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_reindexPipeline->getPipelineLayout()->getHandle());

        glm::uvec3 numGroups = getNumWorkGroups(glm::uvec3(m_numParticles, 1, 1), *m_reindexPipeline);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);
    }

    void PositionBasedFluid::computeLambdas(VkCommandBuffer cmdBuffer) const
    {
        m_lambdasPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_lambdasPipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, m_numParticles);
        m_lambdasDescGroup.setDynamicOffset(0, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_lambdasDescGroup.setDynamicOffset(1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_lambdasDescGroup.setDynamicOffset(2, m_currentSection * m_numParticles * sizeof(uint32_t));
        m_lambdasDescGroup.setDynamicOffset(3, m_currentSection * m_numParticles * sizeof(float));
        m_lambdasDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_lambdasPipeline->getPipelineLayout()->getHandle());

        glm::uvec3 numGroups = getNumWorkGroups(glm::uvec3(m_numParticles, 1, 1), *m_lambdasPipeline);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);
    }

    void PositionBasedFluid::computeDeltas(VkCommandBuffer cmdBuffer) const
    {
        m_deltasPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_deltasPipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, m_numParticles);
        m_deltasDescGroup.setDynamicOffset(0, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_deltasDescGroup.setDynamicOffset(1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
        m_deltasDescGroup.setDynamicOffset(2, m_currentSection * m_numParticles * sizeof(uint32_t));
        m_deltasDescGroup.setDynamicOffset(3, m_currentSection * m_numParticles * sizeof(float));
        m_deltasDescGroup.setDynamicOffset(4, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_deltasDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_deltasPipeline->getPipelineLayout()->getHandle());

        glm::uvec3 numGroups = getNumWorkGroups(glm::uvec3(m_numParticles, 1, 1), *m_deltasPipeline);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);
    }

    void PositionBasedFluid::updatePositions(VkCommandBuffer cmdBuffer) const
    {
        m_updatePosPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        m_updatePosPipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_numParticles);
        m_updatePosDescGroup.setDynamicOffset(0, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_updatePosDescGroup.setDynamicOffset(1, m_currentSection * m_numParticles * sizeof(glm::vec4));
        m_updatePosDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_updatePosPipeline->getPipelineLayout()->getHandle());

        glm::uvec3 numGroups = getNumWorkGroups(glm::uvec3(m_numParticles, 1, 1), *m_updatePosPipeline);
        vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        insertComputeBarrier(cmdBuffer);
    }

    void PositionBasedFluid::updateAll(VkCommandBuffer /*cmdBuffer*/, float /*timeDelta*/) const
    {
    }

    void PositionBasedFluid::addVorticity(VkCommandBuffer /*cmdBuffer*/) const
    {
    }

    void PositionBasedFluid::insertComputeBarrier(VkCommandBuffer cmdBuffer) const
    {
        VkMemoryBarrier memBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);
    }

    std::vector<glm::vec4> PositionBasedFluid::createInitialPositions(glm::uvec3 fluidDim, float particleRadius) const
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