#pragma once

#include <vector>

#include "Renderer/VertexBuffer.hpp"

namespace crisp
{
    struct VertexBufferBindingGroup
    {
        VertexBufferBindingGroup() {}
        ~VertexBufferBindingGroup() {}

        using VertexBufferBindingGroupItem = std::pair<VkBuffer, VkDeviceSize>;

        VertexBufferBindingGroup(uint32_t firstBinding, uint32_t bindingCount, std::initializer_list<VertexBufferBindingGroupItem>&& bindings)
            : firstBinding(firstBinding)
            , bindingCount(bindingCount)
        {
            for (auto& item : bindings)
            {
                buffers.emplace_back(item.first);
                offsets.emplace_back(item.second);
            }
        }

        VertexBufferBindingGroup(uint32_t bindingCount, std::initializer_list<VertexBufferBindingGroupItem>&& bindings)
            : VertexBufferBindingGroup(0, bindingCount, std::forward<decltype(bindings)&&>(bindings))
        {
        }

        VertexBufferBindingGroup(std::initializer_list<VertexBufferBindingGroupItem>&& bindings)
            : VertexBufferBindingGroup(0, static_cast<uint32_t>(bindings.size()), std::forward<decltype(bindings)&&>(bindings))
        {
        }

        inline void bind(const VkCommandBuffer& cmdBuffer) const
        {
            vkCmdBindVertexBuffers(cmdBuffer, firstBinding, bindingCount, buffers.data(), offsets.data());
        }

        uint32_t firstBinding;
        uint32_t bindingCount;
        std::vector<VkBuffer>     buffers;
        std::vector<VkDeviceSize> offsets;
    };
}