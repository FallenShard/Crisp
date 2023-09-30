// #pragma once
//
// #include <memory>
//
// #include <Crisp/Math/Headers.hpp>
//
// #include "Renderer/DescriptorSetGroup.hpp"
//
// #include "Renderer/Renderer.hpp"
// #include "FluidSimulation.hpp"
// #include "vulkan/VulkanBuffer.hpp"
//
//  namespace crisp
//{
//     class VulkanPipeline;
//     class Renderer;
//     class VulkanDevice;
//     class UniformBuffer;
//
//     class PositionBasedFluid : public FluidSimulation
//     {
//     public:
//         PositionBasedFluid(Renderer* renderer);
//         virtual ~PositionBasedFluid();
//
//         virtual void update(float dt);
//
//         virtual void dispatchCompute(VkCommandBuffer cmdBuffer, uint32_t currentFrameIdx) const override;
//
//         virtual void onKeyPressed(Key key, int modifier) override;
//         virtual void setGravityX(float value) override;
//         virtual void setGravityY(float value) override;
//         virtual void setGravityZ(float value) override;
//         virtual void setViscosity(float value) override;
//         virtual void setSurfaceTension(float value) override;
//         virtual void reset();
//
//         virtual float getParticleRadius() const override;
//
//         virtual void drawGeometry(VkCommandBuffer cmdBuffer) const override;
//
//     private:
//         void predictPositions(VkCommandBuffer cmdBuffer, float timeDelta) const;
//         void clearCellCounts(VkCommandBuffer cmdBuffer) const;
//         void computeCellCounts(VkCommandBuffer cmdBuffer) const;
//         void scanCellCounts(VkCommandBuffer cmdBuffer) const;
//         void reindex(VkCommandBuffer cmdBuffer) const;
//         void computeLambdas(VkCommandBuffer cmdBuffer) const;
//         void computeDeltas(VkCommandBuffer cmdBuffer) const;
//         void updatePositions(VkCommandBuffer cmdBuffer) const;
//         void updateAll(VkCommandBuffer cmdBuffer, float timeDelta) const;
//         void addVorticity(VkCommandBuffer cmdBuffer) const;
//
//         void insertComputeBarrier(VkCommandBuffer cmdBuffer) const;
//
//         std::vector<glm::vec4> createInitialPositions(glm::uvec3 fluidDim, float particleRadius) const;
//
//         Renderer* m_renderer;
//         VulkanDevice* m_device;
//
//         uint32_t m_numParticles;
//         std::unique_ptr<VulkanBuffer> m_positions;
//         std::unique_ptr<VulkanBuffer> m_colors;
//         std::unique_ptr<VulkanBuffer> m_indices;
//         std::unique_ptr<VulkanBuffer> m_velocities;
//         std::unique_ptr<VulkanBuffer> m_pressures;
//         std::unique_ptr<VulkanBuffer> m_forces;
//         std::unique_ptr<VulkanBuffer> m_reorderedPositions;
//         std::unique_ptr<VulkanBuffer> m_lambdas;
//         std::unique_ptr<VulkanBuffer> m_deltas;
//
//         struct GridParams
//         {
//             glm::uvec3 dim;
//             uint32_t numCells;
//             glm::vec3 spaceSize;
//             float cellSize;
//         };
//
//         glm::vec3 m_fluidSpaceMin;
//         glm::vec3 m_fluidSpaceMax;
//         GridParams m_gridParams;
//         std::unique_ptr<VulkanBuffer> m_cellCounts;
//         std::unique_ptr<VulkanBuffer> m_cellIndices;
//         std::unique_ptr<VulkanBuffer> m_blockSums;
//         uint32_t m_blockSumRegionSize;
//
//         float m_particleRadius;
//         glm::uvec3 m_fluidDim;
//
//         float m_timeDelta;
//         mutable bool m_runCompute;
//
//         mutable uint32_t m_prevSection;
//         mutable uint32_t m_currentSection;
//
//         std::unique_ptr<VulkanPipeline> m_predictPositionsPipeline;
//         DescriptorSetGroup m_predictPosDescGroup;
//
//         std::unique_ptr<VulkanPipeline> m_clearHashGridPipeline;
//         DescriptorSetGroup m_clearGridDescGroup;
//
//         std::unique_ptr<VulkanPipeline> m_computeCellCountPipeline;
//         DescriptorSetGroup m_computeCellCountDescGroup;
//
//         std::unique_ptr<VulkanPipeline> m_scanPipeline;
//         DescriptorSetGroup m_scanDescGroup;
//         DescriptorSetGroup m_scanBlockDescGroup;
//
//         std::unique_ptr<VulkanPipeline> m_combineScanPipeline;
//         DescriptorSetGroup m_combineScanDescGroup;
//
//         std::unique_ptr<VulkanPipeline> m_reindexPipeline;
//         DescriptorSetGroup m_reindexDescGroup;
//
//         std::unique_ptr<VulkanPipeline> m_lambdasPipeline;
//         DescriptorSetGroup m_lambdasDescGroup;
//
//         std::unique_ptr<VulkanPipeline> m_deltasPipeline;
//         DescriptorSetGroup m_deltasDescGroup;
//
//         std::unique_ptr<VulkanPipeline> m_updatePosPipeline;
//         DescriptorSetGroup m_updatePosDescGroup;
//
//         std::unique_ptr<VulkanPipeline> m_forcesPipeline;
//         DescriptorSetGroup m_forcesDescGroup;
//
//         float m_viscosityFactor = 5.0f;
//         float m_kappa = 1.0f;
//         glm::vec3 m_gravity = { 0.0f, -9.81f, 0.0f };
//         bool m_runSimulation = false;
//     };
// }