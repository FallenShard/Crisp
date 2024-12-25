#pragma once

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanMemoryHeap.hpp>
#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/VulkanResource.hpp>

namespace crisp {
class VulkanAccelerationStructure final : public VulkanResource<VkAccelerationStructureKHR> {
public:
    VulkanAccelerationStructure(
        const VulkanDevice& device,
        const VkAccelerationStructureGeometryKHR& geometry,
        uint32_t primitiveCount,
        const glm::mat4& transform);
    explicit VulkanAccelerationStructure(
        const VulkanDevice& device, const std::vector<VulkanAccelerationStructure*>& bottomLevelAccelStructures = {});

    void setPrimitiveCount(uint32_t count);
    void setBuildRangeOffsets(uint32_t primitiveOffset, uint32_t firstVertex);

    void build(VkCommandBuffer cmdBuffer);

    VkWriteDescriptorSetAccelerationStructureKHR getDescriptorInfo() const;

private:
    void createAccelerationStructure(const VulkanDevice& device);

    // VRAM address of the acceleration structure object. Used for building TLAS from BLAS addresses.
    VkDeviceAddress m_address;

    // Transform matrix is used for transforming the BLASes to help with instancing.
    VkTransformMatrixKHR m_transformMatrix;

    // Describes the geometry of AS primitives - a variant of triangles, AABB or instances.
    VkAccelerationStructureGeometryKHR m_geometry;

    // TLAS will be build from multiple BLASes, held by this vector.
    std::vector<VkAccelerationStructureInstanceKHR> m_instances;

    VkAccelerationStructureBuildGeometryInfoKHR m_buildInfo;
    VkAccelerationStructureBuildSizesInfoKHR m_buildSizes;
    VkAccelerationStructureBuildRangeInfoKHR m_buildRange;

    std::unique_ptr<VulkanBuffer> m_accelerationStructureBuffer;
    std::unique_ptr<VulkanBuffer> m_scratchBuffer;
    std::unique_ptr<VulkanBuffer> m_instanceBuffer;
};
} // namespace crisp