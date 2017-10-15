#include "FluidSimulation.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/Pipelines/PointSphereSpritePipeline.hpp"
#include "Renderer/Pipelines/ComputeTestPipeline.hpp"
#include "Renderer/Pipelines/HashGridPipeline.hpp"
#include "Renderer/Pipelines/ClearHashGridPipeline.hpp"
#include "Renderer/Pipelines/ComputePressurePipeline.hpp"
#include "Renderer/UniformBuffer.hpp"


#include "Models/BoxDrawer.hpp"

#include "Vulkan/VulkanRenderPass.hpp"
#include "vulkan/VulkanBuffer.hpp"
#include "vulkan/VulkanDevice.hpp"

#include <iostream>
#include <numeric>

#include "Models/FluidSimulationParams.hpp"


#include "Simulation/FluidSimulationKernels.cuh"

#include "glfw/glfw3.h"

namespace crisp
{
    namespace
    {
        bool run = false;
    }

    FluidSimulation::FluidSimulation(VulkanRenderer* renderer, VulkanRenderPass* renderPass)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
        , m_particleRadius(particleRadius)
        , m_runCompute(true)
        , m_time(0.0f)
        , m_gravity(0.0f, -9.81f, 0.0f)
    {
        m_workGroupSize = { 1, 1, 1 };
        m_fluidDim      = m_workGroupSize * glm::uvec3(16, 32, 16);
        m_numParticles  = m_fluidDim.x * m_fluidDim.y * m_fluidDim.z;
        m_fluidSpaceMin = glm::vec3(0.0f);
        m_fluidSpaceMax = glm::vec3(m_fluidDim) * 2.0f * particleDiameter;

        m_gridParams.cellSize  = h;
        m_gridParams.spaceSize = m_fluidSpaceMax - m_fluidSpaceMin;
        m_gridParams.dim = glm::ivec3(glm::ceil((m_fluidSpaceMax - m_fluidSpaceMin) / m_gridParams.cellSize));

        std::vector<glm::vec4> positions;
        for (int y = 0; y < m_fluidDim.y; ++y)
            for (int z = 0; z < m_fluidDim.z; ++z)
                for (int x = 0; x < m_fluidDim.x; ++x) {
                    glm::vec3 translation = glm::vec3(m_fluidDim) * m_particleRadius;
                    translation.x = 0.0f;
                    glm::vec3 offset = glm::vec3(x, y, z) * particleDiameter + particleRadius + translation;
                    positions.emplace_back(offset, 1.0f);
                }

        m_positions = positions;
                    

        m_colors = std::vector<glm::vec4>(positions.size(), glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));

        m_posBuffer = std::make_unique<PropertyBuffer<glm::vec4>>(m_numParticles);
        m_posBuffer->setHostBuffer(positions);
        m_posBuffer->copyToGpu();
        
        m_gridLocationBuffer = std::make_unique<PropertyBuffer<int>>(m_numParticles);

        std::vector<int> particleIndices(m_numParticles);
        std::iota(particleIndices.begin(), particleIndices.end(), 0);
        m_particleIndexBuffer = std::make_unique<PropertyBuffer<int>>(m_numParticles);
        m_particleIndexBuffer->setHostBuffer(particleIndices);
        m_particleIndexBuffer->copyToGpu();

        m_colorCudaBuffer = std::make_unique<PropertyBuffer<glm::vec4>>(m_numParticles);
        m_colorCudaBuffer->setHostBuffer(m_colors);
        m_colorCudaBuffer->copyToGpu();

        m_densityCudaBuffer = std::make_unique<PropertyBuffer<float>>(m_numParticles);
        m_pressureCudaBuffer = std::make_unique<PropertyBuffer<float>>(m_numParticles);
        m_veloBuffer = std::make_unique<PropertyBuffer<glm::vec3>>(m_numParticles);
        std::vector<glm::vec3> velocities(m_numParticles, glm::vec3(0.0f));
        m_veloBuffer->setHostBuffer(velocities);
        m_veloBuffer->copyToGpu();
        m_forceBuffer = std::make_unique<PropertyBuffer<glm::vec3>>(m_numParticles);
        

        int numCells = m_gridParams.dim.x * m_gridParams.dim.y * m_gridParams.dim.z;
        std::vector<int> gridCellIds(numCells);
        std::iota(gridCellIds.begin(), gridCellIds.end(), 0);
        m_gridCellIndexBuffer = std::make_unique<PropertyBuffer<int>>(numCells);
        m_gridCellIndexBuffer->setHostBuffer(gridCellIds);
        m_gridCellIndexBuffer->copyToGpu();

        m_gridCellOffsets = std::make_unique<PropertyBuffer<int>>(numCells);
        m_gridCellOffsets->setHostBuffer(gridCellIds);
        m_gridCellOffsets->copyToGpu();


        m_grid.resize(m_gridParams.dim.y * m_gridParams.dim.z * m_gridParams.dim.x);
        for (auto& cell : m_grid)
            cell.reserve(1024);

        m_gridCounts.resize(m_gridParams.dim.y * m_gridParams.dim.z * m_gridParams.dim.x, 0);

        m_densities.resize(m_numParticles);
        m_pressures.resize(m_numParticles);
        m_forces.resize(m_numParticles);
        m_velocities.resize(m_numParticles, glm::vec3(0.0f));

        m_gridParamsBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(GridParams), BufferUpdatePolicy::Constant, &m_gridParams);


        m_vertexBufferSize = positions.size() * sizeof(glm::vec4);
        m_vertexBuffer = std::make_unique<VulkanBuffer>(m_device, 3 * m_vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_renderer->fillDeviceBuffer(m_vertexBuffer.get(), positions.data(), m_vertexBufferSize);
        m_stagingBuffer = std::make_unique<VulkanBuffer>(m_device, m_vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        m_colorBuffer = std::make_unique<VulkanBuffer>(m_device, 3 * m_vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_stagingColorBuffer = std::make_unique<VulkanBuffer>(m_device, m_vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        m_renderer->fillDeviceBuffer(m_colorBuffer.get(), m_colors.data(), m_vertexBufferSize);

        //VkDeviceSize gridBufferSize = m_grid.size() * sizeof(glm::uvec4);
        //m_gridCellsBuffer = std::make_unique<VulkanBuffer>(m_device, gridBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        //m_renderer->fillDeviceBuffer(m_gridCellsBuffer.get(), m_grid.data(), gridBufferSize);
        //
        //VkDeviceSize gridCountSize = m_gridCounts.size() * sizeof(uint32_t);
        //m_gridCellCountsBuffer = std::make_unique<VulkanBuffer>(m_device, gridCountSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        //m_renderer->fillDeviceBuffer(m_gridCellCountsBuffer.get(), m_gridCounts.data(), gridCountSize);

        VkDeviceSize pressureSize = positions.size() * sizeof(float);
        m_pressureBuffer = std::make_unique<VulkanBuffer>(m_device, pressureSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(Transforms), BufferUpdatePolicy::PerFrame);
        m_paramsBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(ParticleParams), BufferUpdatePolicy::PerFrame);

        m_drawPipeline = std::make_unique<PointSphereSpritePipeline>(m_renderer, renderPass);
        m_descriptorSetGroup =
        {
            m_drawPipeline->allocateDescriptorSet(0),
            m_drawPipeline->allocateDescriptorSet(1)
        };
        m_descriptorSetGroup.postBufferUpdate(0, 0, m_transformsBuffer->getDescriptorInfo());
        m_descriptorSetGroup.postBufferUpdate(1, 0, m_paramsBuffer->getDescriptorInfo());
        m_descriptorSetGroup.flushUpdates(m_device);

        m_particleParams.radius = m_particleRadius;
        m_particleParams.screenSpaceScale = 1.0f;
        m_transformsBuffer->updateStagingBuffer(&m_particleParams, sizeof(ParticleParams));

        m_compPipeline = std::make_unique<ComputeTestPipeline>(m_renderer);

        m_compSetGroup = { m_compPipeline->allocateDescriptorSet(0) };

        VkDescriptorBufferInfo descInfo = {};
        descInfo.buffer = m_vertexBuffer->getHandle();
        descInfo.offset = 0;
        descInfo.range  = positions.size() * sizeof(glm::vec4);

        m_compSetGroup.postBufferUpdate(0, 0, descInfo);
        m_compSetGroup.postBufferUpdate(0, 1, { m_colorBuffer->getHandle(), 0, m_colors.size() * sizeof(glm::vec4) });
        //m_compSetGroup.postBufferUpdate(0, 2, { m_gridCellsBuffer->getHandle(),      0, gridBufferSize });
        //m_compSetGroup.postBufferUpdate(0, 3, { m_gridCellCountsBuffer->getHandle(), 0, gridCountSize });
        m_compSetGroup.postBufferUpdate(0, 4, { m_gridParamsBuffer->get(), 0, sizeof(GridParams) });
        m_compSetGroup.postBufferUpdate(0, 5, { m_pressureBuffer->getHandle(),       0, pressureSize });
        m_compSetGroup.flushUpdates(m_device);

        m_hashGridPipeline = std::make_unique<HashGridPipeline>(m_renderer);
        m_hashGridSets = { m_hashGridPipeline->allocateDescriptorSet(0) };

        m_hashGridSets.postBufferUpdate(0, 0, descInfo);
        //m_hashGridSets.postBufferUpdate(0, 1, { m_gridCellsBuffer->getHandle(),      0, gridBufferSize });
        //m_hashGridSets.postBufferUpdate(0, 2, { m_gridCellCountsBuffer->getHandle(), 0, gridCountSize });
        m_hashGridSets.postBufferUpdate(0, 3, { m_gridParamsBuffer->get(), 0, sizeof(GridParams) });
        m_hashGridSets.flushUpdates(m_device);

        m_clearGridPipeline = std::make_unique<ClearHashGridPipeline>(m_renderer);
        m_clearGridSets = { m_clearGridPipeline->allocateDescriptorSet(0) };

        //m_clearGridSets.postBufferUpdate(0, 0, { m_gridCellsBuffer->getHandle(),      0, gridBufferSize });
        //m_clearGridSets.postBufferUpdate(0, 1, { m_gridCellCountsBuffer->getHandle(), 0, gridCountSize });
        m_clearGridSets.flushUpdates(m_device);

        m_pressurePipeline = std::make_unique<ComputePressurePipeline>(m_renderer);
        m_pressureSets = { m_pressurePipeline->allocateDescriptorSet(0) };
        m_pressureSets.postBufferUpdate(0, 0, descInfo);
        m_pressureSets.postBufferUpdate(0, 1, { m_pressureBuffer->getHandle(),       0, pressureSize });
        //m_pressureSets.postBufferUpdate(0, 2, { m_gridCellsBuffer->getHandle(),      0, gridBufferSize });
        //m_pressureSets.postBufferUpdate(0, 3, { m_gridCellCountsBuffer->getHandle(), 0, gridCountSize });
        m_pressureSets.postBufferUpdate(0, 4, { m_gridParamsBuffer->get(), 0, sizeof(GridParams) });
        m_pressureSets.flushUpdates(m_device);


        m_time = 2.0f;

        m_boxDrawer = std::make_unique<BoxDrawer>(m_renderer, 1, renderPass);
        std::vector<glm::vec3> centers;
        std::vector<glm::vec3> scales;
        centers.push_back(m_fluidSpaceMax / 2.0f * vizScale);
        scales.push_back(m_fluidSpaceMax * vizScale);
        //for (int y = 0; y < m_gridParams.dim.y; ++y)
        //    for (int z = 0; z < m_gridParams.dim.z; ++z)
        //        for (int x = 0; x < m_gridParams.dim.x; ++x)
        //        {
        //            centers.push_back(glm::vec3(x * h, y * h, z * h) + glm::vec3(h / 2.0f));
        //            scales.push_back(h);
        //        }
        m_boxDrawer->setBoxTransforms(centers, scales);

        m_viscosity = 3.0f;

        m_simulationParams = std::make_unique<SimulationParams>();
        m_simulationParams->numParticles = m_numParticles;
        m_simulationParams->gridDims = m_gridParams.dim;

        m_simulationParams->cellSize   = h;
        m_simulationParams->poly6Const = 315.0f / (64.0f * PI * h3 * h3 * h3);
    }

    FluidSimulation::~FluidSimulation()
    {
    }

    void FluidSimulation::onKeyPressed(int code, int modifier)
    {
        if (code == GLFW_KEY_SPACE)
            run = !run;
    }

    void FluidSimulation::update(const glm::mat4& V, const glm::mat4& P, float dt)
    {
        m_boxDrawer->update(V, P);
        m_transforms.M = glm::scale(glm::vec3(vizScale));
        m_transforms.MV  = V * m_transforms.M;
        m_transforms.MVP = P * m_transforms.MV;
        m_transformsBuffer->updateStagingBuffer(&m_transforms, sizeof(m_transforms));

        m_particleParams.radius = particleRadius * vizScale;
        m_particleParams.screenSpaceScale = m_renderer->getSwapChainExtent().width * P[0][0];
        m_paramsBuffer->updateStagingBuffer(&m_particleParams, sizeof(m_particleParams));

        m_time += dt;
        m_runCompute = true;


        //for (int y = 0; y < m_fluidDim.y; ++y)
        //    for (int z = 0; z < m_fluidDim.z; ++z)
        //        for (int x = 0; x < m_fluidDim.x; ++x)
        //        {
        //            glm::vec3 translation = glm::vec3(m_fluidDim) * m_particleRadius;
        //            glm::vec3 offset = glm::vec3(x, y, z) * m_particleRadius * 2.0f + m_particleRadius + translation;
        //            size_t index = getIndex(x, y, z);
        //            m_positions[index] = glm::vec4(offset, 1.0f);
        //        }

        //for (auto& v : m_gridCounts) {
        //    v = 0;
        //}
        //
        //
        //for (int i = 0; i < m_positions.size(); i++) {
        //    uint32_t gridIdx = getGridIndex(m_positions[i]);
        //    uint32_t gridSubIndex = m_gridCounts[gridIdx];
        //
        //    m_gridCounts[gridIdx]++;
        //    m_grid[gridIdx][gridSubIndex] = i;
        //}

        //for (auto& v : m_gridCounts) {
        //    v = 0;
        //}
        //
        //for (int i = 0; i < m_numParticles; i++)
        //{
        //
        //}
        //
        //for (int i = 0; i < m_numParticles; i++)
        //{
        //    glm::vec3 position = m_positions[i];
        //    float density = 0.0f;
        //    for (int j = 0; j < m_numParticles; j++)
        //    {
        //        glm::vec3 nbr = m_positions[j];
        //        glm::vec3 diff = position - nbr;
        //        density += mass * poly6(glm::length(diff));
        //    }
        //
        //    m_densities[i] = density;
        //    //m_pressures[i] = stiffness * (m_densities[i] - restDensity);
        //    m_pressures[i] = std::max(0.0f, stiffness * (m_densities[i] - restDensity));
        //}
        //
        //for (int i = 0; i < m_numParticles; i++)
        //{
        //    glm::vec3 position = m_positions[i];
        //    glm::vec3 fPressure(0.0f);
        //    glm::vec3 fGravity = m_densities[i] * g;
        //    for (int j = 0; j < m_numParticles; j++)
        //    {
        //        glm::vec3 nbrPosition = m_positions[j];
        //
        //        glm::vec3 diffVec = position - nbrPosition;
        //        float dist = glm::length(diffVec);
        //        if (dist > 0.0f)
        //            fPressure += -mass / m_densities[j] * (m_pressures[i] + m_pressures[j]) / 2.0f * poly6Grad(dist) * diffVec / dist;
        //    }
        //    m_forces[i] = fPressure + fGravity;
        //}
        //
        //static constexpr float deltaT = 0.0016f;
        //for (int i = 0; i < m_numParticles; i++)
        //{
        //    glm::vec3 a = m_forces[i] / m_densities[i];
        //    m_velocities[i] = m_velocities[i] + dt * a;
        //    m_positions[i] = m_positions[i] + glm::vec4(dt * m_velocities[i], 0.0f);
        //    m_positions[i].y = std::max(m_positions[i].y, particleRadius);
        //    m_positions[i].x = std::max(std::min(m_positions[i].x, 10.0f - particleRadius), particleRadius);
        //    m_positions[i].z = std::max(std::min(m_positions[i].z, 10.0f - particleRadius), particleRadius);
        //}

        ////float minDensity = 1000.0f;
        ////float maxDensity = 1000.0f;
        //for (int i = 0; i < m_positions.size(); i++) {
        //    glm::vec3 position = m_positions[i];
        //
        //    for (int j = 0; j < m_positions.size(); j++) {
        //        glm::vec3 nbrPosition = m_positions[j];
        //
        //        float dist = glm::length(nbrPosition - position);
        //        float normDist = dist / h;
        //        density += mass * cubicSpline(normDist);
        //    }
        //
        //    float density = getDensity(m_positions[i]);
        //    //minDensity = std::min(minDensity, density);
        //    //maxDensity = std::max(maxDensity, density);
        //    m_densities[i] = density;
        //    m_pressures[i] = stiffness * (pow(density / restDensity, 7.0f) - 1.0f);
        //}
        //
        ////std::cout << minDensity << " " << maxDensity << std::endl;
        //
        //constexpr float deltaT = 0.001f;
        //
        //for (int i = 0; i < m_numParticles; i++)
        //{
        //    glm::vec3 grad = getPressureGrad(i);
        //    glm::vec3 fPress = -mass / m_densities[i] * grad; // TODO use grad
        //    glm::vec3 fOther = mass * g;
        //    m_forces[i] = fPress;// fOther + fPress;
        //
        //    glm::vec3 dv = 10.0f * m_forces[i] / mass;
        //
        //    //std::cout << grad.x << ", " << grad.y << ", " << grad.z << std::endl;
        //
        //    m_velocities[i] = m_velocities[i] + deltaT * dv;
        //    m_positions[i] = m_positions[i] + glm::vec4(deltaT * m_velocities[i], 0.0f);
        //}

        //for (int i = 0; i < 5; i++) {
        //    assignParticlesToGrids();
        //    computeDensityAndPressure();
        //    computeForces();
        //    integrate();
        //}

        // compute grid index for each particle
        for (int i = 0; i < 5 && run; i++)
        {
            int3 gridParams = { m_gridParams.dim.x, m_gridParams.dim.y, m_gridParams.dim.z };
            FluidSimulationKernels::computeGridLocations(*m_gridLocationBuffer, *m_particleIndexBuffer, *m_posBuffer, *m_simulationParams);

            FluidSimulationKernels::sortParticleIndicesByGridLocation(*m_gridLocationBuffer, *m_particleIndexBuffer);

            FluidSimulationKernels::computeGridCellOffsets(*m_gridCellOffsets, *m_gridLocationBuffer, *m_gridCellIndexBuffer);

            FluidSimulationKernels::computeDensityAndPressure(*m_densityCudaBuffer, *m_pressureCudaBuffer,
                *m_posBuffer, *m_gridCellOffsets, *m_gridLocationBuffer, *m_particleIndexBuffer,
                *m_simulationParams);

            FluidSimulationKernels::computeForces(*m_forceBuffer, *m_colorCudaBuffer,
                *m_posBuffer, *m_gridCellOffsets, *m_gridLocationBuffer, *m_particleIndexBuffer,
                *m_densityCudaBuffer, *m_pressureCudaBuffer, *m_veloBuffer, gridParams, m_gridParams.cellSize, m_gravity, m_viscosity);



            float3 worldSpace;
            worldSpace.x = m_fluidSpaceMax.x;
            worldSpace.y = m_fluidSpaceMax.y;
            worldSpace.z = m_fluidSpaceMax.z;
            FluidSimulationKernels::integrate(*m_posBuffer, *m_veloBuffer, *m_forceBuffer, *m_densityCudaBuffer, worldSpace, dt * timeStepFraction);

        }

        m_colorCudaBuffer->copyToCpu();
        m_posBuffer->copyToCpu();

        m_stagingBuffer->updateFromHost(m_posBuffer->getHostBuffer());
        m_stagingColorBuffer->updateFromHost(m_colorCudaBuffer->getHostBuffer());
    }

    void FluidSimulation::updateDeviceBuffers(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex)
    {
        m_transformsBuffer->updateDeviceBuffer(cmdBuffer, currentFrameIndex);
        m_paramsBuffer->updateDeviceBuffer(cmdBuffer, currentFrameIndex);


        m_vertexBuffer->copyFrom(cmdBuffer, *m_stagingBuffer, 0, currentFrameIndex * m_vertexBufferSize, m_vertexBufferSize);
        m_colorBuffer->copyFrom(cmdBuffer, *m_stagingColorBuffer, 0, currentFrameIndex * m_vertexBufferSize, m_vertexBufferSize);

        m_boxDrawer->updateDeviceBuffers(cmdBuffer, currentFrameIndex);
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
        m_viscosity = value;
    }

    void FluidSimulation::reset()
    {
        run = false;

        std::vector<glm::vec4> positions;
        for (int y = 0; y < m_fluidDim.y; ++y)
            for (int z = 0; z < m_fluidDim.z; ++z)
                for (int x = 0; x < m_fluidDim.x; ++x) {
                    glm::vec3 translation = glm::vec3(m_fluidDim) * m_particleRadius;
                    translation.x = 0.0f;
                    glm::vec3 offset = glm::vec3(x, y, z) * particleDiameter + particleRadius + translation;
                    positions.emplace_back(offset, 1.0f);
                }

        m_positions = positions;

        m_posBuffer->setHostBuffer(positions);
        m_posBuffer->copyToGpu();

        std::vector<glm::vec3> velocities(m_numParticles, glm::vec3(0.0f));
        m_veloBuffer->setHostBuffer(velocities);
        m_veloBuffer->copyToGpu();
    }

    void FluidSimulation::draw(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex) const
    {
        m_descriptorSetGroup.setDynamicOffset(0, m_transformsBuffer->getDynamicOffset(currentFrameIndex));
        m_descriptorSetGroup.setDynamicOffset(1, m_paramsBuffer->getDynamicOffset(currentFrameIndex));
        m_drawPipeline->bind(cmdBuffer);
        m_descriptorSetGroup.bind(cmdBuffer, m_drawPipeline->getPipelineLayout());

        VkDeviceSize offsets[] = { currentFrameIndex * m_vertexBufferSize, currentFrameIndex * m_vertexBufferSize };
        VkBuffer buffers[] = { m_vertexBuffer->getHandle(), m_colorBuffer->getHandle() };
        vkCmdBindVertexBuffers(cmdBuffer, 0, 2, buffers, offsets);
        vkCmdDraw(cmdBuffer, m_numParticles, 1, 0, 0);

        m_boxDrawer->render(cmdBuffer, currentFrameIndex);
    }

    void FluidSimulation::clearHashGrid(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex) const
    {
        m_clearGridPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        VkDescriptorSet set = m_clearGridSets.getHandle(0);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_clearGridPipeline->getPipelineLayout(), 0, 1, &set, 0, nullptr);

        vkCmdDispatch(cmdBuffer, m_gridParams.dim.x / m_workGroupSize.x, m_gridParams.dim.y / m_workGroupSize.y, m_gridParams.dim.z / m_workGroupSize.z);
    }
    void FluidSimulation::buildHashGrid(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex) const
    {
        m_hashGridPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        VkDescriptorSet set = m_hashGridSets.getHandle(0);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_hashGridPipeline->getPipelineLayout(), 0, 1, &set, 0, nullptr);

        vkCmdDispatch(cmdBuffer, m_fluidDim.x / m_workGroupSize.x, m_fluidDim.y / m_workGroupSize.y, m_fluidDim.z / m_workGroupSize.z);
    }

    void FluidSimulation::computePressure(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex) const
    {
        m_pressurePipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        VkDescriptorSet set = m_pressureSets.getHandle(0);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pressurePipeline->getPipelineLayout(), 0, 1, &set, 0, nullptr);

        vkCmdDispatch(cmdBuffer, m_fluidDim.x / m_workGroupSize.x, m_fluidDim.y / m_workGroupSize.y, m_fluidDim.z / m_workGroupSize.z);
    }
}