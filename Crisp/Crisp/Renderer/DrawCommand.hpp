#pragma once

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Geometry/GeometryView.hpp>
#include <Crisp/Renderer/Material.hpp>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

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
};

namespace detail {
using DrawFunc = void (*)(VkCommandBuffer, const GeometryViewVariant&);

inline void draw(VkCommandBuffer cmdBuffer, const GeometryViewVariant& geomView) {
    const auto& view = std::get<ListGeometryView>(geomView);
    vkCmdDraw(cmdBuffer, view.vertexCount, view.instanceCount, view.firstVertex, view.firstInstance);
}

inline void drawIndexed(VkCommandBuffer cmdBuffer, const GeometryViewVariant& geomView) {
    const auto& view = std::get<IndexedGeometryView>(geomView);
    vkCmdBindIndexBuffer(cmdBuffer, view.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(
        cmdBuffer, view.indexCount, view.instanceCount, view.firstIndex, view.vertexOffset, view.firstInstance);
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
    VkViewport viewport = {};
    VkRect2D scissor = {};
    VulkanPipeline* pipeline;
    Material* material;
    std::vector<uint32_t> dynamicBufferOffsets;

    PushConstantView pushConstantView;

    Geometry* geometry;
    GeometryViewVariant geometryView;
    detail::DrawFunc drawFunc;
    uint32_t firstBuffer;
    uint32_t bufferCount;

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