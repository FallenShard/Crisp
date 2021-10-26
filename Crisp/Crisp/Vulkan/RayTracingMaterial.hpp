#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "VulkanBuffer.hpp"

#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
    class Renderer;
    class UniformBuffer;

    class Geometry;

    class RayTracingMaterial
    {
    public:
        RayTracingMaterial(Renderer* renderer, VkAccelerationStructureNV tlas, std::array<VkImageView, 2> writeImageViews, const UniformBuffer& cameraBuffer);

        void bind(VkCommandBuffer cmdBuffer, uint32_t virtualFrameIndex) const;

        void updateGeometryBufferDescriptors(const Geometry& geometry, uint32_t idx);
        void setRandomBuffer(const UniformBuffer& randBuffer);

        VkExtent2D getExtent() const;

        VulkanBuffer* getShaderBindingTableBuffer() const;

        uint32_t getShaderGroupHandleSize() const;

        void resetFrameCounter();

    private:
        void createPipeline(Renderer* renderer);

        Renderer* m_renderer;

        VkPipeline m_pipeline;
        VkPipelineLayout m_pipelineLayout;

        VkDescriptorPool m_pool;

        std::vector<VkDescriptorSet> m_descSets;

        std::vector<VkDescriptorSetLayout> m_setLayouts;

        std::unique_ptr<VulkanBuffer> m_sbtBuffer;

        mutable uint32_t m_frameIdx;
    };
}