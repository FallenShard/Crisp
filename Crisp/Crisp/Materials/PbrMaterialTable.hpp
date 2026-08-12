#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanStagingBelt.hpp>

namespace crisp {

struct PbrMaterialHandle {
    uint32_t index{0};
};

// Passed per draw. The shader follows materialTableAddress as a physical-storage-buffer pointer and selects
// materials[materialIndex], so the table needs no descriptor set.
struct PbrDrawParameters {
    VkDeviceAddress materialTableAddress{0};
    uint32_t materialIndex{0};
    uint32_t padding{0};
};

static_assert(sizeof(PbrDrawParameters) == 16);

class PbrMaterialTable {
public:
    PbrMaterialTable(VulkanDevice& device, uint32_t capacity);

    PbrMaterialHandle add(const PbrParams& params);
    PbrDrawParameters createDrawParameters(PbrMaterialHandle handle) const;

    // Uploads only the populated prefix when the CPU table changed. The staging belt keeps the upload alive until
    // the frame's timeline value retires it.
    void updateDeviceBuffer(VulkanStagingBelt& stagingBelt, const VulkanCommandEncoder& encoder);

    uint32_t getMaterialCount() const {
        return m_materialCount;
    }

    uint32_t getCapacity() const {
        return static_cast<uint32_t>(m_materials.size());
    }

    VkDeviceAddress getDeviceAddress() const {
        return m_buffer->getDeviceAddress();
    }

private:
    std::vector<PbrParams> m_materials;
    std::unique_ptr<VulkanBuffer> m_buffer;
    uint32_t m_materialCount{0};
    bool m_isDirty{false};
};

} // namespace crisp
