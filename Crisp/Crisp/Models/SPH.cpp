#include "SPH.hpp"

#include <Crisp/Renderer/Renderer.hpp>

#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>


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

    SPH::SPH(Renderer* renderer, RenderGraph* renderGraph)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
        , m_particleRadius(0.01f)
        , m_currentSection(0)
        , m_prevSection(0)
        , m_timeDelta(0.0f)
        , m_renderGraph(renderGraph)
    {
        m_fluidDim = glm::uvec3(32, 64, 32);
        m_numParticles = m_fluidDim.x * m_fluidDim.y * m_fluidDim.z;

        std::vector<glm::vec4> positions = createInitialPositions(m_fluidDim, m_particleRadius);

        const VkDeviceSize vertexBufferSize = m_numParticles * sizeof(glm::vec4);
        m_vertexBuffer = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_renderer->fillDeviceBuffer(m_vertexBuffer.get(), positions.data(), vertexBufferSize, 0);
        m_renderer->fillDeviceBuffer(m_vertexBuffer.get(), positions.data(), vertexBufferSize, vertexBufferSize);

        auto colors = std::vector<glm::vec4>(m_numParticles, glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));
        m_colorBuffer = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_renderer->fillDeviceBuffer(m_colorBuffer.get(), colors.data(), vertexBufferSize, 0);

        m_reorderedPositionBuffer = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_fluidSpaceMin = glm::vec3(0.0f);
        //m_fluidSpaceMax = glm::vec3(m_fluidDim) * 2.0f * 2.0f * m_particleRadius;
        m_fluidSpaceMax = glm::vec3(m_fluidDim.x, m_fluidDim.y / 2, m_fluidDim.z / 2) * 4.0f * m_particleRadius;
        m_gridParams.cellSize = 4 * m_particleRadius;
        m_gridParams.spaceSize = m_fluidSpaceMax - m_fluidSpaceMin;
        m_gridParams.dim = glm::ivec3(glm::ceil((m_fluidSpaceMax - m_fluidSpaceMin) / m_gridParams.cellSize));
        m_gridParams.numCells = m_gridParams.dim.x * m_gridParams.dim.y * m_gridParams.dim.z;

        m_cellCountBuffer = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_gridParams.numCells * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_cellIdBuffer = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_indexBuffer = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_blockSumRegionSize = static_cast<uint32_t>(std::max(m_gridParams.numCells / ScanElementsPerBlock * sizeof(uint32_t), 32ull));
        m_blockSumBuffer = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_blockSumRegionSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_densityBuffer = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(float),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_pressureBuffer = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(float),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_velocityBuffer = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(glm::vec4),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        auto velocities = std::vector<glm::vec4>(m_numParticles, glm::vec4(glm::vec3(0.0f), 1.0f));
        m_renderer->fillDeviceBuffer(m_velocityBuffer.get(), velocities.data(), vertexBufferSize, 0);
        m_forcesBuffer = std::make_unique<VulkanBuffer>(m_device, Renderer::NumVirtualFrames * m_numParticles * sizeof(glm::vec4),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // Clear Hash Grid
        auto& clearHashGrid = renderGraph->addComputePass("clear-hash-grid");
        clearHashGrid.isEnabled = false;
        clearHashGrid.workGroupSize = glm::ivec3(256, 1, 1);
        clearHashGrid.numWorkGroups = (glm::ivec3(m_gridParams.numCells, 1, 1) - 1) / clearHashGrid.workGroupSize + 1;
        clearHashGrid.pipeline = createComputePipeline(m_renderer, "clear-hash-grid.comp", 1, 1, sizeof(uint32_t), clearHashGrid.workGroupSize);
        clearHashGrid.material = std::make_unique<Material>(clearHashGrid.pipeline.get());

        // Output
        clearHashGrid.material->writeDescriptor(0, 0, { m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });

        renderGraph->addDependency("clear-hash-grid", "compute-cell-count", [this](const VulkanRenderPass& src, VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
            std::array<VkBufferMemoryBarrier, 1> barriers;
            for (auto& barrier : barriers)
            {
                barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = m_gridParams.numCells * sizeof(uint32_t);
                barrier.offset = m_currentSection * m_gridParams.numCells * sizeof(uint32_t);
            }
            barriers[0].buffer = m_cellCountBuffer->getHandle();

            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, barriers.size(), barriers.data(), 0, nullptr);
        });



        // Compute particle count per cell
        auto& computeCellCount = renderGraph->addComputePass("compute-cell-count");
        computeCellCount.isEnabled = false;
        computeCellCount.workGroupSize = glm::ivec3(256, 1, 1);
        computeCellCount.numWorkGroups = (glm::ivec3(m_numParticles, 1, 1) - 1) / computeCellCount.workGroupSize + 1;
        computeCellCount.pipeline = createComputePipeline(m_renderer, "compute-cell-count.comp", 3, 1, sizeof(glm::uvec3) + sizeof(float) + sizeof(uint32_t), computeCellCount.workGroupSize);
        computeCellCount.material = std::make_unique<Material>(computeCellCount.pipeline.get());

        // Input
        computeCellCount.material->writeDescriptor(0, 0, { m_vertexBuffer->getHandle(),    0, vertexBufferSize });

        // Output
        computeCellCount.material->writeDescriptor(0, 1, { m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        computeCellCount.material->writeDescriptor(0, 2, { m_cellIdBuffer->getHandle(),    0, m_numParticles * sizeof(uint32_t) });

        renderGraph->addDependency("compute-cell-count", "scan", [this](const VulkanRenderPass& src, VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
            std::array<VkBufferMemoryBarrier, 2> barriers;
            for (auto& barrier : barriers)
            {
                barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = m_gridParams.numCells * sizeof(uint32_t);
                barrier.offset = m_currentSection * m_gridParams.numCells * sizeof(uint32_t);
            }
            barriers[0].buffer = m_cellCountBuffer->getHandle();

            barriers[1].size = m_numParticles * sizeof(uint32_t);
            barriers[1].offset = m_currentSection * m_numParticles * sizeof(uint32_t);
            barriers[1].buffer = m_cellIdBuffer->getHandle();

            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, barriers.size(), barriers.data(), 0, nullptr);
        });


        // Scan for individual blocks
        auto& scan = renderGraph->addComputePass("scan");
        scan.isEnabled = false;
        scan.workGroupSize = glm::ivec3(256, 1, 1);
        scan.numWorkGroups = (glm::ivec3(m_gridParams.numCells / ScanElementsPerThread, 1, 1) - 1) / scan.workGroupSize + 1;
        scan.pipeline = createComputePipeline(m_renderer, "scan.comp", 2, 1, sizeof(int32_t) + sizeof(uint32_t), scan.workGroupSize);
        scan.material = std::make_unique<Material>(scan.pipeline.get());

        // Input/Output
        scan.material->writeDescriptor(0, 0, { m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        scan.material->writeDescriptor(0, 1, { m_blockSumBuffer->getHandle(), 0, m_blockSumRegionSize });

        renderGraph->addDependency("scan", "scan-block", [this](const VulkanRenderPass& src, VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
            std::array<VkBufferMemoryBarrier, 1> barriers;
            for (auto& barrier : barriers)
            {
                barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = m_blockSumRegionSize;
                barrier.offset = m_currentSection * m_blockSumRegionSize;
            }
            barriers[0].buffer = m_blockSumBuffer->getHandle();

            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, barriers.size(), barriers.data(), 0, nullptr);
        });

        // Scan for the block sums
        auto& scanBlock = renderGraph->addComputePass("scan-block");
        scanBlock.isEnabled = false;
        scanBlock.workGroupSize = glm::ivec3(256, 1, 1);
        scanBlock.numWorkGroups = (glm::ivec3(m_gridParams.numCells / ScanElementsPerBlock / ScanElementsPerThread, 1, 1) - 1) / scanBlock.workGroupSize + 1;
        scanBlock.pipeline = createComputePipeline(m_renderer, "scan.comp", 2, 1, sizeof(int32_t) + sizeof(uint32_t), scanBlock.workGroupSize);
        scanBlock.material = std::make_unique<Material>(scanBlock.pipeline.get());

        // Input/Output
        scanBlock.material->writeDescriptor(0, 0, { m_blockSumBuffer->getHandle(), 0, m_blockSumRegionSize });
        scanBlock.material->writeDescriptor(0, 1, { m_blockSumBuffer->getHandle(), 0, m_blockSumRegionSize });

        renderGraph->addDependency("scan-block", "scan-combine", [this](const VulkanRenderPass& src, VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
            std::array<VkBufferMemoryBarrier, 1> barriers;
            for (auto& barrier : barriers)
            {
                barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = m_blockSumRegionSize;
                barrier.offset = m_currentSection * m_blockSumRegionSize;
            }
            barriers[0].buffer = m_blockSumBuffer->getHandle();

            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, barriers.size(), barriers.data(), 0, nullptr);
        });

        // Add block prefix sum to intra-block prefix sums
        auto& scanCombine = renderGraph->addComputePass("scan-combine");
        scanCombine.isEnabled = false;
        scanCombine.workGroupSize = glm::ivec3(512, 1, 1);
        scanCombine.numWorkGroups = (glm::ivec3(m_gridParams.numCells, 1, 1) - 1) / scanCombine.workGroupSize + 1;
        scanCombine.pipeline = createComputePipeline(m_renderer, "scan-combine.comp", 2, 1, sizeof(uint32_t), scanCombine.workGroupSize);
        scanCombine.material = std::make_unique<Material>(scanCombine.pipeline.get());

        // Input/Output
        scanCombine.material->writeDescriptor(0, 0, { m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        scanCombine.material->writeDescriptor(0, 1, { m_blockSumBuffer->getHandle(),  0, m_blockSumRegionSize });

        renderGraph->addDependency("scan-combine", "reindex", [this](const VulkanRenderPass& src, VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
            std::array<VkBufferMemoryBarrier, 2> barriers;
            for (auto& barrier : barriers)
            {
                barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = m_gridParams.numCells * sizeof(uint32_t);
                barrier.offset = m_currentSection * m_gridParams.numCells * sizeof(uint32_t);
            }
            barriers[0].buffer = m_cellCountBuffer->getHandle();

            barriers[1].size = m_blockSumRegionSize;
            barriers[1].offset = m_currentSection * m_blockSumRegionSize;
            barriers[1].buffer = m_blockSumBuffer->getHandle();
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, barriers.size(), barriers.data(), 0, nullptr);
        });


        auto& reindex = renderGraph->addComputePass("reindex");
        reindex.isEnabled = false;
        reindex.workGroupSize = glm::ivec3(256, 1, 1);
        reindex.numWorkGroups = (glm::ivec3(m_numParticles, 1, 1) - 1) / reindex.workGroupSize + 1;
        reindex.pipeline = createComputePipeline(m_renderer, "reindex-particles.comp", 5, 1, sizeof(GridParams) + sizeof(float), reindex.workGroupSize);
        reindex.material = std::make_unique<Material>(reindex.pipeline.get());

        // Input
        reindex.material->writeDescriptor(0, 0, { m_vertexBuffer->getHandle(),    0, vertexBufferSize });
        reindex.material->writeDescriptor(0, 1, { m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        reindex.material->writeDescriptor(0, 2, { m_cellIdBuffer->getHandle(),    0, m_numParticles * sizeof(uint32_t) });

        // Output
        reindex.material->writeDescriptor(0, 3, { m_indexBuffer->getHandle(),   0, m_numParticles * sizeof(uint32_t) });
        reindex.material->writeDescriptor(0, 4, { m_reorderedPositionBuffer->getHandle(),  0, vertexBufferSize });

        renderGraph->addDependency("reindex", "compute-density-and-pressure", [this](const VulkanRenderPass& src, VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
            std::array<VkBufferMemoryBarrier, 2> barriers;
            for (auto& barrier : barriers)
            {
                barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = m_numParticles * sizeof(uint32_t);
                barrier.offset = m_currentSection * m_numParticles * sizeof(uint32_t);
            }
            barriers[0].buffer = m_indexBuffer->getHandle();
            barriers[1].size = m_numParticles * sizeof(glm::vec4);
            barriers[1].offset = m_currentSection * m_numParticles * sizeof(glm::vec4);
            barriers[1].buffer = m_reorderedPositionBuffer->getHandle();
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, barriers.size(), barriers.data(), 0, nullptr);
        });

        auto& computePressure = renderGraph->addComputePass("compute-density-and-pressure");
        computePressure.isEnabled = false;
        computePressure.workGroupSize = glm::ivec3(256, 1, 1);
        computePressure.numWorkGroups = (glm::ivec3(m_numParticles, 1, 1) - 1) / computePressure.workGroupSize + 1;
        computePressure.pipeline = createComputePipeline(m_renderer, "compute-density-and-pressure.comp", 5, 1, sizeof(GridParams) + sizeof(float), computePressure.workGroupSize);
        computePressure.material = std::make_unique<Material>(computePressure.pipeline.get());

        // Input
        computePressure.material->writeDescriptor(0, 0, { m_vertexBuffer->getHandle(),    0, vertexBufferSize });
        computePressure.material->writeDescriptor(0, 1, { m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        computePressure.material->writeDescriptor(0, 2, { m_reorderedPositionBuffer->getHandle(),     0, vertexBufferSize });

        // Output
        computePressure.material->writeDescriptor(0, 3, { m_densityBuffer->getHandle(),   0, m_numParticles * sizeof(float) });
        computePressure.material->writeDescriptor(0, 4, { m_pressureBuffer->getHandle(),  0, m_numParticles * sizeof(float) });

        renderGraph->addDependency("compute-density-and-pressure", "compute-forces", [this](const VulkanRenderPass& src, VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
            std::array<VkBufferMemoryBarrier, 2> barriers;
            for (auto& barrier : barriers)
            {
                barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = m_numParticles * sizeof(float);
                barrier.offset = m_currentSection * m_numParticles * sizeof(float);
            }
            barriers[0].buffer = m_densityBuffer->getHandle();
            barriers[1].buffer = m_pressureBuffer->getHandle();
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, barriers.size(), barriers.data(), 0, nullptr);
        });

        auto& computeForces = renderGraph->addComputePass("compute-forces");
        computeForces.isEnabled = false;
        computeForces.workGroupSize = glm::ivec3(256, 1, 1);
        computeForces.numWorkGroups = (glm::ivec3(m_numParticles, 1, 1) - 1) / computeForces.workGroupSize + 1;
        computeForces.pipeline = createComputePipeline(m_renderer, "compute-forces.comp", 7, 1, sizeof(GridParams) + sizeof(glm::uvec3) + sizeof(uint32_t) + 2 * sizeof(float), computeForces.workGroupSize);
        computeForces.material = std::make_unique<Material>(computeForces.pipeline.get());

        // Input
        computeForces.material->writeDescriptor(0, 0, { m_vertexBuffer->getHandle(),    0, vertexBufferSize });
        computeForces.material->writeDescriptor(0, 1, { m_cellCountBuffer->getHandle(), 0, m_gridParams.numCells * sizeof(uint32_t) });
        computeForces.material->writeDescriptor(0, 2, { m_indexBuffer->getHandle(),     0, m_numParticles * sizeof(uint32_t) });
        computeForces.material->writeDescriptor(0, 3, { m_densityBuffer->getHandle(),   0, m_numParticles * sizeof(float) });
        computeForces.material->writeDescriptor(0, 4, { m_pressureBuffer->getHandle(),  0, m_numParticles * sizeof(float) });
        computeForces.material->writeDescriptor(0, 5, { m_velocityBuffer->getHandle(),  0, vertexBufferSize });

        // Output
        computeForces.material->writeDescriptor(0, 6, { m_forcesBuffer->getHandle(),    0, vertexBufferSize });

        renderGraph->addDependency("compute-forces", "integrate", [this, vertexBufferSize](const VulkanRenderPass& src, VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
            std::array<VkBufferMemoryBarrier, 1> barriers;
            for (auto& barrier : barriers)
            {
                barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.size = vertexBufferSize;
                barrier.offset = m_currentSection * vertexBufferSize;
            }
            barriers[0].buffer = m_forcesBuffer->getHandle();
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, barriers.size(), barriers.data(), 0, nullptr);
        });

        auto& integrateNode = renderGraph->addComputePass("integrate");
        integrateNode.isEnabled = false;
        integrateNode.workGroupSize = glm::ivec3(256, 1, 1);
        integrateNode.numWorkGroups = (glm::ivec3(m_numParticles, 1, 1) - 1) / integrateNode.workGroupSize + 1;
        integrateNode.pipeline = createComputePipeline(m_renderer, "integrate.comp", 6, 1, sizeof(GridParams) + sizeof(uint32_t) + sizeof(float), integrateNode.workGroupSize);
        integrateNode.material = std::make_unique<Material>(integrateNode.pipeline.get());

        // Input
        integrateNode.material->writeDescriptor(0, 0, { m_vertexBuffer->getHandle(),    0, vertexBufferSize });
        integrateNode.material->writeDescriptor(0, 1, { m_velocityBuffer->getHandle(),  0, vertexBufferSize });
        integrateNode.material->writeDescriptor(0, 2, { m_forcesBuffer->getHandle(),    0, vertexBufferSize });

        // Output
        integrateNode.material->writeDescriptor(0, 3, { m_vertexBuffer->getHandle(),    0, vertexBufferSize });
        integrateNode.material->writeDescriptor(0, 4, { m_velocityBuffer->getHandle(),  0, vertexBufferSize });
        integrateNode.material->writeDescriptor(0, 5, { m_colorBuffer->getHandle(),     0, vertexBufferSize });
        renderGraph->addDependency("integrate", "mainPass", [this, vertexBufferSize](const VulkanRenderPass& src, VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
            std::array<VkBufferMemoryBarrier, 2> barriers;
            for (auto& barrier : barriers)
            {
                barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
                barrier.size = vertexBufferSize;
                barrier.offset = m_currentSection * vertexBufferSize;
            }
            barriers[0].buffer = m_vertexBuffer->getHandle();
            barriers[1].buffer = m_colorBuffer->getHandle();
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, barriers.size(), barriers.data(), 0, nullptr);
        });
    }

    SPH::~SPH()
    {
    }

    void SPH::update(float dt)
    {
        if (!m_runSimulation)
            return;

        m_timeDelta = dt;

        m_prevSection = m_currentSection;
        m_currentSection = (m_currentSection + 1) % Renderer::NumVirtualFrames;

        m_renderGraph->getNode("clear-hash-grid").isEnabled = true;
        m_renderGraph->getNode("clear-hash-grid").preDispatchCallback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIdx) {
            auto& pass = m_renderGraph->getNode("clear-hash-grid");
            pass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams.numCells);

            pass.material->setDynamicOffset(frameIdx, 0, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));

            pass.isEnabled = false;
        };

        m_renderGraph->getNode("compute-cell-count").isEnabled = true;
        m_renderGraph->getNode("compute-cell-count").preDispatchCallback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIdx) {
            auto& pass = m_renderGraph->getNode("compute-cell-count");
            pass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams.dim, m_gridParams.cellSize, m_numParticles);

            pass.material->setDynamicOffset(frameIdx, 0, m_prevSection * m_numParticles * sizeof(glm::vec4));

            pass.material->setDynamicOffset(frameIdx, 1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
            pass.material->setDynamicOffset(frameIdx, 2, m_currentSection * m_numParticles * sizeof(uint32_t));

            pass.isEnabled = false;
        };

        m_renderGraph->getNode("scan").isEnabled = true;
        m_renderGraph->getNode("scan").preDispatchCallback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIdx) {
            auto& pass = m_renderGraph->getNode("scan");
            pass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 1, m_gridParams.numCells);

            pass.material->setDynamicOffset(frameIdx, 0, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
            pass.material->setDynamicOffset(frameIdx, 1, m_currentSection * m_blockSumRegionSize);

            pass.isEnabled = false;
        };

        m_renderGraph->getNode("scan-block").isEnabled = true;
        m_renderGraph->getNode("scan-block").preDispatchCallback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIdx) {
            auto& pass = m_renderGraph->getNode("scan-block");
            pass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_gridParams.numCells / ScanElementsPerBlock);

            pass.material->setDynamicOffset(frameIdx, 0, m_currentSection * m_blockSumRegionSize);
            pass.material->setDynamicOffset(frameIdx, 1, m_currentSection * m_blockSumRegionSize);

            pass.isEnabled = false;
        };

        m_renderGraph->getNode("scan-combine").isEnabled = true;
        m_renderGraph->getNode("scan-combine").preDispatchCallback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIdx) {
            auto& pass = m_renderGraph->getNode("scan-combine");
            pass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams.numCells);

            pass.material->setDynamicOffset(frameIdx, 0, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
            pass.material->setDynamicOffset(frameIdx, 1, m_currentSection * m_blockSumRegionSize);

            pass.isEnabled = false;
        };

        m_renderGraph->getNode("compute-density-and-pressure").isEnabled = true;
        m_renderGraph->getNode("compute-density-and-pressure").preDispatchCallback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIdx) {
            auto& pass = m_renderGraph->getNode("compute-density-and-pressure");
            pass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, m_numParticles);

            pass.material->setDynamicOffset(frameIdx, 0, m_prevSection * m_numParticles * sizeof(glm::vec4));

            pass.material->setDynamicOffset(frameIdx, 1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
            pass.material->setDynamicOffset(frameIdx, 2, m_currentSection * m_numParticles * sizeof(glm::vec4));
            pass.material->setDynamicOffset(frameIdx, 3, m_currentSection * m_numParticles * sizeof(float));
            pass.material->setDynamicOffset(frameIdx, 4, m_currentSection * m_numParticles * sizeof(float));

            pass.isEnabled = false;
        };

        m_renderGraph->getNode("reindex").isEnabled = true;
        m_renderGraph->getNode("reindex").preDispatchCallback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIdx) {
            auto& pass = m_renderGraph->getNode("reindex");
            pass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, m_numParticles);

            pass.material->setDynamicOffset(frameIdx, 0, m_prevSection * m_numParticles * sizeof(glm::vec4));

            pass.material->setDynamicOffset(frameIdx, 1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
            pass.material->setDynamicOffset(frameIdx, 2, m_currentSection * m_numParticles * sizeof(uint32_t));
            pass.material->setDynamicOffset(frameIdx, 3, m_currentSection * m_numParticles * sizeof(uint32_t));
            pass.material->setDynamicOffset(frameIdx, 4, m_currentSection * m_numParticles * sizeof(glm::vec4));

            pass.isEnabled = false;
        };

        m_renderGraph->getNode("compute-density-and-pressure").isEnabled = true;
        m_renderGraph->getNode("compute-density-and-pressure").preDispatchCallback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIdx) {
            auto& pass = m_renderGraph->getNode("compute-density-and-pressure");
            pass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, m_numParticles);

            pass.material->setDynamicOffset(frameIdx, 0, m_prevSection * m_numParticles * sizeof(glm::vec4));

            pass.material->setDynamicOffset(frameIdx, 1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
            pass.material->setDynamicOffset(frameIdx, 2, m_currentSection * m_numParticles * sizeof(glm::vec4));
            pass.material->setDynamicOffset(frameIdx, 3, m_currentSection * m_numParticles * sizeof(float));
            pass.material->setDynamicOffset(frameIdx, 4, m_currentSection * m_numParticles * sizeof(float));

            pass.isEnabled = false;
        };

        m_renderGraph->getNode("compute-forces").isEnabled = true;
        m_renderGraph->getNode("compute-forces").preDispatchCallback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIdx) {
            auto& pass = m_renderGraph->getNode("compute-forces");
            pass.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, m_gravity, m_numParticles, m_viscosityFactor, m_kappa);

            pass.material->setDynamicOffset(frameIdx, 0, m_prevSection * m_numParticles * sizeof(glm::vec4));
            pass.material->setDynamicOffset(frameIdx, 5, m_prevSection * m_numParticles * sizeof(glm::vec4));

            pass.material->setDynamicOffset(frameIdx, 1, m_currentSection * m_gridParams.numCells * sizeof(uint32_t));
            pass.material->setDynamicOffset(frameIdx, 2, m_currentSection * m_numParticles * sizeof(uint32_t));
            pass.material->setDynamicOffset(frameIdx, 3, m_currentSection * m_numParticles * sizeof(float));
            pass.material->setDynamicOffset(frameIdx, 4, m_currentSection * m_numParticles * sizeof(float));
            pass.material->setDynamicOffset(frameIdx, 6, m_currentSection * m_numParticles * sizeof(glm::vec4));

            pass.isEnabled = false;
        };

        m_renderGraph->getNode("integrate").isEnabled = true;
        m_renderGraph->getNode("integrate").preDispatchCallback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIdx) {
            auto& node = m_renderGraph->getNode("integrate");
            node.pipeline->setPushConstants(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, m_gridParams, m_timeDelta / 2.0f, m_numParticles);

            for (uint32_t i = 0; i <= 1; ++i)
                node.material->setDynamicOffset(frameIdx, i, m_prevSection * m_numParticles * sizeof(glm::vec4));

            for (uint32_t i = 2; i <= 5; ++i)
                node.material->setDynamicOffset(frameIdx, i, m_currentSection * m_numParticles * sizeof(glm::vec4));


            node.isEnabled = false;
        };
    }

    void SPH::dispatchCompute(VkCommandBuffer cmdBuffer, uint32_t currentFrameIdx) const
    {
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

    float SPH::getParticleRadius() const
    {
        return m_particleRadius;
    }

    void SPH::drawGeometry(VkCommandBuffer cmdBuffer) const
    {
        VkDeviceSize offset = m_currentSection * m_numParticles * sizeof(glm::vec4);
        VkDeviceSize offsets[] = { offset, offset };
        VkBuffer buffers[] = { m_vertexBuffer->getHandle(), m_colorBuffer->getHandle() };
        vkCmdBindVertexBuffers(cmdBuffer, 0, 2, buffers, offsets);
        vkCmdDraw(cmdBuffer, m_numParticles, 1, 0, 0);
    }

    VulkanBuffer* SPH::getVertexBuffer(std::string_view key) const
    {
        if (key == "position")
            return m_vertexBuffer.get();
        else if (key == "color")
            return m_colorBuffer.get();
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

    std::vector<glm::vec4> SPH::createInitialPositions(glm::uvec3 fluidDim, float particleRadius) const
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