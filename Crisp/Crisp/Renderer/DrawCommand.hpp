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
#include <variant>
#include <vector>

namespace crisp {
using GeometryViewVariant = std::variant<ListGeometryView, IndexedGeometryView>;

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

namespace detail {
using DrawFunc = void (*)(const VulkanCommandEncoder&, const GeometryViewVariant&);

inline void draw(const VulkanCommandEncoder& encoder, const GeometryViewVariant& geomView) {
    const auto& view = std::get<ListGeometryView>(geomView);
    encoder.draw(view.vertexCount, view.instanceCount, view.firstVertex, view.firstInstance);
}

inline void drawIndexed(const VulkanCommandEncoder& encoder, const GeometryViewVariant& geomView) {
    const auto& view = std::get<IndexedGeometryView>(geomView);
    encoder.bindIndexBuffer(view.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    encoder.drawIndexed(view.indexCount, view.instanceCount, view.firstIndex, view.vertexOffset, view.firstInstance);
}

template <typename GeometryView>
constexpr DrawFunc getDrawFunc() {
    if constexpr (std::is_same_v<GeometryView, IndexedGeometryView>) {
        return drawIndexed;
    } else if constexpr (std::is_same_v<GeometryView, ListGeometryView>) {
        return draw;
    } else {
        return nullptr;
    }
}
} // namespace detail

struct DrawCommand {
    static constexpr uint32_t kMaxDynamicBufferOffsets = 4;

    VkViewport viewport = {};
    VkRect2D scissor = {};
    VulkanPipeline* pipeline;
    Material* material;
    std::array<uint32_t, kMaxDynamicBufferOffsets> dynamicBufferOffsets{};
    uint32_t dynamicBufferOffsetCount{0};

    PushConstantView pushConstantView;

    Geometry* geometry;
    GeometryViewVariant geometryView;
    detail::DrawFunc drawFunc;
    uint32_t firstBuffer;
    uint32_t bufferCount;

    std::span<const uint32_t> getDynamicBufferOffsets() const {
        return std::span{dynamicBufferOffsets}.first(dynamicBufferOffsetCount);
    }

    template <typename GeometryView, typename... Args>
    void setGeometryView(Args&&... args) {
        geometryView = GeometryView(std::forward<Args>(args)...);
        drawFunc = detail::getDrawFunc<GeometryView>();
    }

    template <typename GeometryView>
    void setGeometryView(GeometryView&& view) {
        geometryView = std::forward<GeometryView>(view);
        drawFunc = detail::getDrawFunc<GeometryView>();
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
