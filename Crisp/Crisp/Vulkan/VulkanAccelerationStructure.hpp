#pragma once

#include <Crisp/Vulkan/VulkanMemoryHeap.hpp>
#include <Crisp/Vulkan/VulkanResource.hpp>

#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
class Geometry;
class VulkanBuffer;
class VulkanDevice;
class VulkanCommandPool;

class VulkanAccelerationStructure : public VulkanResource<VkAccelerationStructureKHR, nullptr>
{
public:
    VulkanAccelerationStructure(const VulkanDevice& device, const VkAccelerationStructureGeometryKHR& geometry,
        uint32_t primitiveCount, const glm::mat4& transform);
    VulkanAccelerationStructure(const VulkanDevice& device,
        const std::vector<VulkanAccelerationStructure*>& bottomLevelAccelStructures = {});

    ~VulkanAccelerationStructure();

    void build(const VulkanDevice& device, VkCommandBuffer cmdBuffer,
        const std::vector<VulkanAccelerationStructure*>& bottomLevelAccelStructures);

    VkWriteDescriptorSetAccelerationStructureKHR getDescriptorInfo() const;

private:
    void createAccelerationStructure(const VulkanDevice& device);

    uint64_t m_rawHandle;
    VkDeviceAddress m_address;
    VulkanMemoryHeap::Allocation m_allocation;
    VkAccelerationStructureInfoNV m_info;

    VkTransformMatrixKHR m_transformMatrx;
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