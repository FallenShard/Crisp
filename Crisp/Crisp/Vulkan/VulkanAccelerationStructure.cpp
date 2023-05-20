#include <Crisp/Vulkan/VulkanAccelerationStructure.hpp>

namespace crisp
{

VulkanAccelerationStructure::VulkanAccelerationStructure(
    const VulkanDevice& device,
    const VkAccelerationStructureGeometryKHR& geometry,
    uint32_t primitiveCount,
    const glm::mat4& transform)
    : VulkanResource(device.getResourceDeallocator())
    , m_geometry(geometry)
    , m_buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR}
    , m_buildSizesInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR}
    , m_primitiveCount(primitiveCount)
{
    m_buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    m_buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    m_buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    m_buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

    memcpy(&m_transformMatrx, glm::value_ptr(glm::transpose(transform)), 12 * sizeof(float));
    m_geometry.geometry.triangles.transformData = {0};

    m_buildInfo.geometryCount = 1;
    m_buildInfo.pGeometries = &m_geometry;

    vkGetAccelerationStructureBuildSizesKHR(
        device.getHandle(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &m_buildInfo,
        &m_primitiveCount,
        &m_buildSizesInfo);

    createAccelerationStructure(device);
}

VulkanAccelerationStructure::VulkanAccelerationStructure(
    const VulkanDevice& device, const std::vector<VulkanAccelerationStructure*>& bottomLevelAccelStructures)
    : VulkanResource(device.getResourceDeallocator())
    , m_buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR}
    , m_primitiveCount(static_cast<uint32_t>(bottomLevelAccelStructures.size()))
    , m_geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR}
    , m_buildSizesInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR}
{
    m_buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    m_buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    m_buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    m_buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

    m_instances.resize(m_primitiveCount, VkAccelerationStructureInstanceKHR{});
    for (uint32_t i = 0; i < m_primitiveCount; ++i)
    {
        m_instances[i].transform = bottomLevelAccelStructures[i]->m_transformMatrx;
        m_instances[i].instanceCustomIndex = i;
        m_instances[i].mask = 0xFF;
        m_instances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        m_instances[i].accelerationStructureReference = bottomLevelAccelStructures[i]->m_address;
    }
    m_instanceBuffer = std::make_unique<VulkanBuffer>(
        device,
        m_primitiveCount * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    m_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    m_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    m_geometry.geometry = {};
    m_geometry.geometry.instances = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    m_geometry.geometry.instances.data.deviceAddress = m_instanceBuffer->getDeviceAddress();
    m_geometry.geometry.instances.arrayOfPointers = VK_FALSE;

    m_buildInfo.geometryCount = 1;
    m_buildInfo.pGeometries = &m_geometry;
    vkGetAccelerationStructureBuildSizesKHR(
        device.getHandle(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &m_buildInfo,
        &m_primitiveCount,
        &m_buildSizesInfo);

    createAccelerationStructure(device);
}

VulkanAccelerationStructure::~VulkanAccelerationStructure() {}

void VulkanAccelerationStructure::build(
    const VulkanDevice& /*device*/,
    VkCommandBuffer cmdBuffer,
    const std::vector<VulkanAccelerationStructure*>& /*bottomLevelAccelStructures*/)
{
    if (m_buildInfo.type == VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR)
    {
        VkAccelerationStructureBuildRangeInfoKHR rangeInfo;
        rangeInfo.firstVertex = 0;
        rangeInfo.primitiveCount = m_primitiveCount;
        rangeInfo.primitiveOffset = 0;
        rangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> rangeInfos{&rangeInfo};

        (void)(cmdBuffer);
        vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &m_buildInfo, rangeInfos.data());
    }
    else
    {
        VkAccelerationStructureBuildRangeInfoKHR rangeInfo;
        rangeInfo.firstVertex = 0;
        rangeInfo.primitiveCount = static_cast<uint32_t>(m_instances.size());
        rangeInfo.primitiveOffset = 0;
        rangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> rangeInfos{&rangeInfo};

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

        vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &m_buildInfo, rangeInfos.data());
    }
}

VkWriteDescriptorSetAccelerationStructureKHR VulkanAccelerationStructure::getDescriptorInfo() const
{
    VkWriteDescriptorSetAccelerationStructureKHR writeInfo = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    writeInfo.accelerationStructureCount = 1;
    writeInfo.pAccelerationStructures = &m_handle;
    return writeInfo;
}

void VulkanAccelerationStructure::createAccelerationStructure(const VulkanDevice& device)
{
    m_accelerationStructureBuffer = std::make_unique<VulkanBuffer>(
        device,
        m_buildSizesInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkAccelerationStructureCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    createInfo.type = m_buildInfo.type;
    createInfo.buffer = m_accelerationStructureBuffer->getHandle();
    createInfo.size = m_buildSizesInfo.accelerationStructureSize;
    vkCreateAccelerationStructureKHR(device.getHandle(), &createInfo, nullptr, &m_handle);
    m_buildInfo.dstAccelerationStructure = m_handle;

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.accelerationStructure = m_handle;
    m_address = vkGetAccelerationStructureDeviceAddressKHR(device.getHandle(), &addressInfo);

    m_scratchBuffer = std::make_unique<VulkanBuffer>(
        device,
        m_buildSizesInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_buildInfo.scratchData.deviceAddress = m_scratchBuffer->getDeviceAddress();
}

} // namespace crisp
