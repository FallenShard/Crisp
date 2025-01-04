#include <Crisp/Vulkan/Rhi/VulkanAccelerationStructure.hpp>

namespace crisp {
namespace {
VkAccelerationStructureBuildSizesInfoKHR getBuildSizes(
    const VkDevice device,
    const VkAccelerationStructureBuildGeometryInfoKHR& buildGeometryInfo,
    const uint32_t primitiveCount) {
    VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(
        device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &primitiveCount, &sizes);
    return sizes;
}
} // namespace

VulkanAccelerationStructure::VulkanAccelerationStructure(
    const VulkanDevice& device,
    const VkAccelerationStructureGeometryKHR& geometry,
    const uint32_t primitiveCount,
    const glm::mat4& transform)
    : VulkanResource(device.getResourceDeallocator())
    , m_address{}
    , m_transformMatrix{}
    , m_geometry(geometry)
    , m_buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR}
    , m_buildSizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR}
    , m_buildRange({.primitiveCount = primitiveCount}) {
    memcpy(&m_transformMatrix, glm::value_ptr(glm::transpose(transform)), sizeof(m_transformMatrix));
    m_geometry.geometry.triangles.transformData.deviceAddress = VkDeviceAddress{0};

    m_buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    m_buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    m_buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    m_buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    m_buildInfo.geometryCount = 1;
    m_buildInfo.pGeometries = &m_geometry;
    m_buildSizes = getBuildSizes(device.getHandle(), m_buildInfo, m_buildRange.primitiveCount);

    createAccelerationStructure(device);
}

VulkanAccelerationStructure::VulkanAccelerationStructure(
    const VulkanDevice& device, const std::vector<VulkanAccelerationStructure*>& bottomLevelAccelStructures)
    : VulkanResource(device.getResourceDeallocator())
    , m_address{}
    , m_transformMatrix{}
    , m_geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR}
    , m_buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR}
    , m_buildSizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR}
    , m_buildRange({.primitiveCount = static_cast<uint32_t>(bottomLevelAccelStructures.size())}) {
    m_instances.resize(m_buildRange.primitiveCount, VkAccelerationStructureInstanceKHR{});
    for (uint32_t i = 0; i < m_buildRange.primitiveCount; ++i) {
        m_instances[i].transform = bottomLevelAccelStructures[i]->m_transformMatrix;
        m_instances[i].instanceCustomIndex = i;
        m_instances[i].mask = 0xFF;
        m_instances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        m_instances[i].accelerationStructureReference = bottomLevelAccelStructures[i]->m_address;
    }
    m_instanceBuffer = std::make_unique<VulkanBuffer>(
        device,
        m_buildRange.primitiveCount * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    m_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    m_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    m_geometry.geometry = {};
    m_geometry.geometry.instances = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    m_geometry.geometry.instances.data.deviceAddress = m_instanceBuffer->getDeviceAddress();
    m_geometry.geometry.instances.arrayOfPointers = VK_FALSE;

    m_buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    m_buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    m_buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    m_buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    m_buildInfo.geometryCount = 1;
    m_buildInfo.pGeometries = &m_geometry;
    m_buildSizes = getBuildSizes(device.getHandle(), m_buildInfo, m_buildRange.primitiveCount);

    createAccelerationStructure(device);
}

void VulkanAccelerationStructure::setPrimitiveCount(const uint32_t count) {
    m_buildRange.primitiveCount = count;
}

void VulkanAccelerationStructure::setBuildRangeOffsets(const uint32_t primitiveOffset, const uint32_t firstVertex) {
    m_buildRange.firstVertex = firstVertex;
    m_buildRange.primitiveOffset = primitiveOffset;
}

void VulkanAccelerationStructure::build(const VkCommandBuffer cmdBuffer) {
    if (m_buildInfo.type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR) {
        vkCmdUpdateBuffer(
            cmdBuffer,
            m_instanceBuffer->getHandle(),
            0,
            m_instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
            m_instances.data());
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            1,
            &barrier,
            0,
            nullptr,
            0,
            nullptr);
    }
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> rangeInfos{&m_buildRange};
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &m_buildInfo, rangeInfos.data());
}

VkWriteDescriptorSetAccelerationStructureKHR VulkanAccelerationStructure::getDescriptorInfo() const {
    VkWriteDescriptorSetAccelerationStructureKHR writeInfo = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    writeInfo.accelerationStructureCount = 1;
    writeInfo.pAccelerationStructures = &m_handle;
    return writeInfo;
}

void VulkanAccelerationStructure::createAccelerationStructure(const VulkanDevice& device) {
    // 1. Set up the buffer to hold the acceleration structure.
    m_accelerationStructureBuffer = std::make_unique<VulkanBuffer>(
        device,
        m_buildSizes.accelerationStructureSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    const VkAccelerationStructureCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = m_accelerationStructureBuffer->getHandle(),
        .size = m_buildSizes.accelerationStructureSize,
        .type = m_buildInfo.type,
    };
    vkCreateAccelerationStructureKHR(device.getHandle(), &createInfo, nullptr, &m_handle);
    m_buildInfo.dstAccelerationStructure = m_handle;

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.accelerationStructure = m_handle;
    m_address = vkGetAccelerationStructureDeviceAddressKHR(device.getHandle(), &addressInfo);

    m_scratchBuffer = std::make_unique<VulkanBuffer>(
        device,
        m_buildSizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_buildInfo.scratchData.deviceAddress = m_scratchBuffer->getDeviceAddress();
}

} // namespace crisp
