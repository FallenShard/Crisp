#pragma once

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanMemoryHeap.hpp>
#include <Crisp/Vulkan/VulkanResource.hpp>

namespace crisp
{
class VulkanAccelerationStructure : public VulkanResource<VkAccelerationStructureKHR>
{
public:
    VulkanAccelerationStructure(
        const VulkanDevice& device,
        const VkAccelerationStructureGeometryKHR& geometry,
        uint32_t primitiveCount,
        const glm::mat4& transform);
    explicit VulkanAccelerationStructure(
        const VulkanDevice& device, const std::vector<VulkanAccelerationStructure*>& bottomLevelAccelStructures = {});

    void build(
        const VulkanDevice& device,
        VkCommandBuffer cmdBuffer,
        const std::vector<VulkanAccelerationStructure*>& bottomLevelAccelStructures);

    VkWriteDescriptorSetAccelerationStructureKHR getDescriptorInfo() const;

private:
    void createAccelerationStructure(const VulkanDevice& device);

    uint64_t m_rawHandle;
    VkDeviceAddress m_address;
    VulkanMemoryHeap::Allocation m_allocation;
    VkAccelerationStructureInfoNV m_info;

    VkTransformMatrixKHR m_transformMatrix;
    VkAccelerationStructureGeometryKHR m_geometry;
    std::vector<VkAccelerationStructureInstanceKHR> m_instances;
    VkAccelerationStructureBuildGeometryInfoKHR m_buildInfo;
    VkAccelerationStructureBuildSizesInfoKHR m_buildSizesInfo;

    uint32_t m_primitiveCount;

    std::unique_ptr<VulkanBuffer> m_accelerationStructureBuffer;
    std::unique_ptr<VulkanBuffer> m_scratchBuffer;
    std::unique_ptr<VulkanBuffer> m_instanceBuffer;
};
} // namespace crisp