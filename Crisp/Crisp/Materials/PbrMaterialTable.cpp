#include <Crisp/Materials/PbrMaterialTable.hpp>

#include <Crisp/Core/Checks.hpp>

namespace crisp {

PbrMaterialTable::PbrMaterialTable(VulkanDevice& device, const uint32_t capacity)
    : m_materials(capacity)
    , m_buffer(
          std::make_unique<VulkanBuffer>(
              device,
              capacity * sizeof(PbrParams),
              VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT,
              BufferMemoryType::GpuOnly)) {
    CRISP_CHECK_GT(capacity, 0);
    CRISP_CHECK_NE(getDeviceAddress(), 0);
    device.setObjectName(m_buffer->getHandle(), "PBR Material Table");
}

PbrMaterialHandle PbrMaterialTable::add(const PbrParams& params) {
    CRISP_CHECK_LT(m_materialCount, m_materials.size(), "PBR material table capacity exhausted.");
    const PbrMaterialHandle handle{m_materialCount++};
    m_materials[handle.index] = params;
    m_isDirty = true;
    return handle;
}

PbrDrawParameters PbrMaterialTable::createDrawParameters(const PbrMaterialHandle handle) const {
    CRISP_CHECK_LT(handle.index, m_materialCount);
    return {
        .materialTableAddress = getDeviceAddress(),
        .materialIndex = handle.index,
    };
}

void PbrMaterialTable::updateDeviceBuffer(VulkanStagingBelt& stagingBelt, const VulkanCommandEncoder& encoder) {
    if (!m_isDirty) {
        return;
    }

    stagingBelt.uploadBuffer(
        encoder, *m_buffer, 0, m_materials.data(), static_cast<VkDeviceSize>(m_materialCount) * sizeof(PbrParams));
    m_isDirty = false;
}

} // namespace crisp
