#pragma once

#include <vector>

#include <Crisp/Geometry/TransformPack.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

namespace crisp {

struct TransformHandle {
#pragma warning(disable : 4201)

    union {
        struct {
            uint16_t index;
            uint16_t generation;
        };

        uint32_t value;
    };

#pragma warning(default : 4201)

    consteval static TransformHandle createInvalidHandle() {
        constexpr auto kMaxValue = std::numeric_limits<uint16_t>::max();
        return {{{kMaxValue, kMaxValue}}};
    }
};

inline bool operator==(const TransformHandle& a, const TransformHandle& b) {
    return (a.value == b.value);
}

inline bool operator!=(const TransformHandle& a, const TransformHandle& b) {
    return (a.value != b.value);
}

inline bool operator<(const TransformHandle& a, const TransformHandle& b) {
    return (a.value < b.value);
}

inline bool operator>(const TransformHandle& a, const TransformHandle& b) {
    return (a.value > b.value);
}

class TransformBuffer {
public:
    TransformBuffer(Renderer* renderer, std::size_t maxTransformCount);

    const VulkanRingBuffer* getUniformBuffer() const;
    VulkanRingBuffer* getUniformBuffer();

    TransformPack& getPack(TransformHandle handle);

    // The buffer is used as UNIFORM_DYNAMIC, hence we only provide info for one transformation.
    VkDescriptorBufferInfo getDescriptorInfo() const {
        return m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack));
    }

    void update(const glm::mat4& V, const glm::mat4& P);
    void updateStagingBuffer(uint32_t regionIndex);

    TransformHandle getNextIndex();

private:
    uint32_t m_activeTransforms;
    std::vector<TransformPack> m_transforms;
    std::unique_ptr<VulkanRingBuffer> m_transformBuffer;
};
} // namespace crisp