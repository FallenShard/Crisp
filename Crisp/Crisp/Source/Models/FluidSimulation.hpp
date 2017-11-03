#pragma once

#include <memory>

#include <cuda_runtime.h>

#include "Renderer/Transforms.hpp"
#include "Renderer/VertexBufferBindingGroup.hpp"
#include "Renderer/DescriptorSetGroup.hpp"

#include "Simulation/PropertyBuffer.hpp"

namespace crisp
{
    class VulkanRenderPass;
    class VulkanRenderer;
    class UniformBuffer;
    class PointSphereSpritePipeline;
    class ComputeTestPipeline;
    class VulkanBuffer;

    class ClearHashGridPipeline;
    class HashGridPipeline;
    class ComputePressurePipeline;
    class IntegratePipeline;

    class BoxDrawer;

    struct SimulationParams;

    class FluidSimulation
    {
    public:
        FluidSimulation(VulkanRenderer* renderer, VulkanRenderPass* renderPass);
        ~FluidSimulation();

        void onKeyPressed(int code, int modifier);

        void update(const glm::mat4& V, const glm::mat4& P, float dt);
        void updateDeviceBuffers(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex);

        void setGravityX(float value);
        void setGravityY(float value);
        void setGravityZ(float value);
        void setViscosity(float value);
        void reset();

        void draw(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex) const;

    private:
        void clearHashGrid(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex) const;
        void buildHashGrid(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex) const;
        void computePressure(VkCommandBuffer cmdBuffer, uint32_t currentFrameIndex) const;

        glm::vec3 m_gravity;
        float m_viscosity;

        VulkanRenderer* m_renderer;
        VulkanDevice*   m_device;

        std::unique_ptr<BoxDrawer> m_boxDrawer;

        std::unique_ptr<VulkanBuffer> m_vertexBuffer;
        std::unique_ptr<VulkanBuffer> m_colorBuffer;
        std::unique_ptr<VulkanBuffer> m_stagingBuffer;
        std::unique_ptr<VulkanBuffer> m_stagingColorBuffer;
        VkDeviceSize m_vertexBufferSize;


        std::vector<glm::vec4> m_positions;
        std::vector<float> m_densities;
        std::vector<float> m_pressures;
        std::vector<glm::vec3> m_forces;
        std::vector<glm::vec3> m_velocities;
        std::vector<glm::vec4> m_colors;

        std::vector<std::vector<int>> m_grid;
        std::vector<int> m_gridCounts;

        std::unique_ptr<PointSphereSpritePipeline> m_drawPipeline;
        DescriptorSetGroup m_descriptorSetGroup;

        Transforms m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;

        struct ParticleParams
        {
            float radius;
            float screenSpaceScale;
        };
        ParticleParams m_particleParams;
        std::unique_ptr<UniformBuffer> m_paramsBuffer;

        float m_particleRadius;
        uint32_t m_numParticles;

        std::unique_ptr<ComputeTestPipeline> m_compPipeline;
        DescriptorSetGroup m_compSetGroup;

        std::unique_ptr<ClearHashGridPipeline> m_clearGridPipeline;
        DescriptorSetGroup m_clearGridSets;

        std::unique_ptr<HashGridPipeline> m_hashGridPipeline;
        DescriptorSetGroup m_hashGridSets;

        std::unique_ptr<ComputePressurePipeline> m_pressurePipeline;
        DescriptorSetGroup m_pressureSets;

        std::unique_ptr<VulkanBuffer> m_gridCellsBuffer;
        std::unique_ptr<VulkanBuffer> m_gridCellCountsBuffer;

        std::unique_ptr<VulkanBuffer> m_pressureBuffer;

        float m_time;
        bool m_runCompute;

        glm::uvec3 m_workGroupSize;
        glm::uvec3 m_fluidDim;
        glm::vec3 m_fluidSpaceMin;
        glm::vec3 m_fluidSpaceMax;

        struct GridParams
        {
            glm::ivec3 dim;
            int padding;
            glm::vec3 spaceSize;
            float cellSize;
        };

        GridParams m_gridParams;
        std::unique_ptr<UniformBuffer> m_gridParamsBuffer;

        std::unique_ptr<SimulationParams> m_simulationParams;

        std::unique_ptr<PropertyBuffer<glm::vec4>> m_posBuffer;
        std::unique_ptr<PropertyBuffer<int>>       m_particleIndexBuffer;
        std::unique_ptr<PropertyBuffer<int>>       m_gridLocationBuffer;
        
        std::unique_ptr<PropertyBuffer<glm::vec4>> m_colorCudaBuffer;
        std::unique_ptr<PropertyBuffer<glm::vec3>> m_veloBuffer;
        std::unique_ptr<PropertyBuffer<glm::vec3>> m_forceBuffer;
        std::unique_ptr<PropertyBuffer<glm::vec4>> m_normalsBuffer;

        std::unique_ptr<PropertyBuffer<int>> m_gridCellIndexBuffer;
        std::unique_ptr<PropertyBuffer<int>> m_gridCellOffsets;

        std::unique_ptr<PropertyBuffer<float>> m_densityCudaBuffer;
        std::unique_ptr<PropertyBuffer<float>> m_pressureCudaBuffer;
    };
}