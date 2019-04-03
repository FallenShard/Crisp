#pragma once

#include <vector>
#include <variant>
#include <vulkan/vulkan.h>

#include "UniformBuffer.hpp"

#include "Material.hpp"
#include "Geometry/Geometry.hpp"
#include "Geometry/GeometryView.hpp"

namespace crisp
{
    using GeometryViewVariant = std::variant<ListGeometryView, IndexedGeometryView>;

    namespace detail
    {
        using DrawFunc = void(*)(VkCommandBuffer, const GeometryViewVariant&);

        inline void draw(VkCommandBuffer cmdBuffer, const GeometryViewVariant& geomView)
        {
            const auto& view = std::get<ListGeometryView>(geomView);
            vkCmdDraw(cmdBuffer, view.vertexCount, view.instanceCount, view.firstVertex, view.firstInstance);
        }

        inline void drawIndexed(VkCommandBuffer cmdBuffer, const GeometryViewVariant& geomView)
        {
            const auto& view = std::get<IndexedGeometryView>(geomView);
            vkCmdBindIndexBuffer(cmdBuffer, view.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmdBuffer, view.indexCount, view.instanceCount, view.firstIndex, view.vertexOffset, view.firstInstance);
        }

        template <typename GeometryView>
        DrawFunc getDrawFunc()
        {
            if constexpr (std::is_same_v<GeometryView, IndexedGeometryView>)
                return drawIndexed;
            else if constexpr(std::is_same_v<GeometryView, ListGeometryView>)
                return draw;
            else
                return nullptr;
        }

        template <unsigned int offset, typename T, typename... Args>
        void insertElements(std::vector<char>& data, T&& value, Args&&... values)
        {
            memcpy(data.data() + offset, &value, sizeof(T));
            if constexpr (sizeof...(Args) > 0)
                insertElements<offset + sizeof(T), Args...>(data, std::forward<Args>(values)...);
        }
    }

    struct DrawCommand
    {
        VkViewport viewport = {};
        VkRect2D scissor = {};
        std::vector<DynamicBufferInfo> dynamicBuffers;
        std::vector<char> pushConstants;
        VulkanPipeline* pipeline;
        Material* material;

        Geometry* geometry;
        GeometryViewVariant geometryView;
        detail::DrawFunc drawFunc;

        template <typename GeometryView, typename... Args>
        void setGeometryView(Args&&... args)
        {
            geometryView = GeometryView(std::forward<Args>(args)...);
            drawFunc = detail::getDrawFunc<GeometryView>();
        }

        template <typename GeometryView>
        void setGeometryView(GeometryView&& view)
        {
            geometryView = std::move(view);
            drawFunc = detail::getDrawFunc<GeometryView>();
        }

        template <typename... Args>
        void setPushConstants(Args&&... values)
        {
            pushConstants.resize(AggregateSizeof<Args...>::value);
            detail::insertElements<0, Args...>(pushConstants, std::forward<Args>(values)...);
        }
    };
}