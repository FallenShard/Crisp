#pragma once

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Geometry/GeometryView.hpp>
#include <Crisp/Renderer/Material.hpp>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace crisp {
struct PushConstantView {
    const void* data = nullptr;
    VkDeviceSize size = 0;

    PushConstantView() = default;

    template <typename T>
    explicit PushConstantView(const T& pushConstant)
        : data(&pushConstant)
        , size(sizeof(T)) {}

    template <typename T>
    void set(const T& pushConstant) {
        data = &pushConstant;
        size = sizeof(T);
    }

    template <typename T, std::size_t Len>
    void set(const std::array<T, Len>& pushConstants) {
        data = pushConstants.data();
        size = Len * sizeof(T);
    }

    void set(const std::vector<unsigned char>& buffer) {
        data = buffer.data();
        size = buffer.size();
    }

    std::span<const std::byte> asSpan() const {
        return {static_cast<const std::byte*>(data), size};
    }
};

struct DrawCommand {
    static constexpr uint32_t kMaxDynamicBufferOffsets = 4;

    Material* material{};
    Geometry* geometry{};
    GeometryView geometryView;
    PushConstantView pushConstantView;
    std::array<uint32_t, kMaxDynamicBufferOffsets> dynamicBufferOffsets{};
    uint8_t dynamicBufferOffsetCount{0};
    uint8_t firstBuffer{0};
    uint8_t bufferCount{0};

    VulkanPipeline* getPipeline() const {
        return material->getPipeline();
    }

    std::span<const uint32_t> getDynamicBufferOffsets() const {
        return std::span{dynamicBufferOffsets}.first(dynamicBufferOffsetCount);
    }

    void draw(const VulkanCommandEncoder& encoder) const {
        if (geometryView.isIndexed()) {
            encoder.bindIndexBuffer(geometryView.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            encoder.drawIndexed(
                geometryView.elementCount,
                geometryView.instanceCount,
                geometryView.firstElement,
                geometryView.vertexOffset,
                geometryView.firstInstance);
        } else {
            encoder.draw(
                geometryView.elementCount,
                geometryView.instanceCount,
                geometryView.firstElement,
                geometryView.firstInstance);
        }
    }

    template <typename T>
    void setPushConstantView(const T& data) {
        pushConstantView.set(data);
    }

    void setPushConstantView(PushConstantView view) {
        pushConstantView = view;
    }
};

} // namespace crisp
