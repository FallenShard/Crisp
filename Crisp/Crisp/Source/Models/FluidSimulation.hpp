#pragma once

#include <memory>

#include "Math/Headers.hpp"

#include "Renderer/VertexBufferBindingGroup.hpp"
#include "Renderer/DescriptorSetGroup.hpp"

#include "Renderer/VulkanRenderer.hpp"

namespace crisp
{
    class VulkanPipeline;
    class ComputePipeline;
    class VulkanRenderer;
    class VulkanDevice;
    class UniformBuffer;

    class FluidSimulation
    {
    public:
        FluidSimulation(VulkanRenderer* renderer);
        ~FluidSimulation();

        void update(float dt);

        void dispatchCompute(VkCommandBuffer cmdBuffer, uint32_t currentFrameIdx) const;

        void onKeyPressed(int code, int modifier);
        void setGravityX(float value);
        void setGravityY(float value);
        void setGravityZ(float value);
        void setViscosity(float value);
        void setSurfaceTension(float value);
        void reset();

        float getParticleRadius() const;

        void drawGeometry(VkCommandBuffer cmdBuffer) const;

    private:
        void clearCellCounts(VkCommandBuffer cmdBuffer) const;
        void computeCellCounts(VkCommandBuffer cmdBuffer) const;
        void scanCellCounts(VkCommandBuffer cmdBuffer) const;
        void reindex(VkCommandBuffer cmdBuffer) const;
        void computeDensityAndPressure(VkCommandBuffer cmdBuffer) const;
        void computeForces(VkCommandBuffer cmdBuffer) const;
        void integrate(VkCommandBuffer cmdBuffer, float timeDelta) const;

        void insertComputeBarrier(VkCommandBuffer cmdBuffer) const;

        std::vector<glm::vec4> createInitialPositions(glm::uvec3 fluidDim, float particleRadius) const;

        VulkanRenderer* m_renderer;
        VulkanDevice*   m_device;
        std::unique_ptr<VulkanBuffer> m_vertexBuffer;
        std::unique_ptr<VulkanBuffer> m_colorBuffer;
        std::unique_ptr<VulkanBuffer> m_indexBuffer;

        std::unique_ptr<VulkanBuffer> m_reorderedPositionBuffer;

        std::unique_ptr<VulkanBuffer> m_cellCountBuffer;
        std::unique_ptr<VulkanBuffer> m_cellIdBuffer;
        std::unique_ptr<VulkanBuffer> m_blockSumBuffer;
        uint32_t m_blockSumRegionSize;

        std::unique_ptr<VulkanBuffer> m_densityBuffer;
        std::unique_ptr<VulkanBuffer> m_pressureBuffer;
        std::unique_ptr<VulkanBuffer> m_velocityBuffer;
        std::unique_ptr<VulkanBuffer> m_forcesBuffer;


        float m_particleRadius;
        glm::uvec3 m_fluidDim;

        float m_timeDelta;
        mutable bool m_runCompute;

        mutable uint32_t m_prevSection;
        mutable uint32_t m_currentSection;
        std::unique_ptr<ComputePipeline> m_integratePipeline;
        DescriptorSetGroup m_integrateDescGroup;

        std::unique_ptr<ComputePipeline> m_clearHashGridPipeline;
        DescriptorSetGroup m_clearGridDescGroup;

        std::unique_ptr<ComputePipeline> m_computeCellCountPipeline;
        DescriptorSetGroup m_computeCellCountDescGroup;

        std::unique_ptr<ComputePipeline> m_scanPipeline;
        DescriptorSetGroup m_scanDescGroup;
        DescriptorSetGroup m_scanBlockDescGroup;

        std::unique_ptr<ComputePipeline> m_combineScanPipeline;
        DescriptorSetGroup m_combineScanDescGroup;

        std::unique_ptr<ComputePipeline> m_reindexPipeline;
        DescriptorSetGroup m_reindexDescGroup;

        std::unique_ptr<ComputePipeline> m_densityPressurePipeline;
        DescriptorSetGroup m_densityPressureDescGroup;

        std::unique_ptr<ComputePipeline> m_forcesPipeline;
        DescriptorSetGroup m_forcesDescGroup;

        struct GridParams
        {
            glm::uvec3 dim;
            uint32_t numCells;
            glm::vec3 spaceSize;
            float cellSize;
        };

        glm::vec3 m_fluidSpaceMin;
        glm::vec3 m_fluidSpaceMax;
        GridParams m_gridParams;

        uint32_t m_numParticles;

        float m_viscosityFactor = 5.0f;
        float m_kappa = 1.0f;
        glm::vec3 m_gravity = { 0.0f, -9.81f, 0.0f };
        bool m_runSimulation = false;
    };
}