#pragma once

#include "Geometry/Geometry.hpp"
#include "Geometry/TransformPack.hpp"

#include "Renderer/Material.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/DrawCommand.hpp"

namespace crisp
{
    struct RenderNode
    {
        Geometry* geometry = nullptr;
        Material* material = nullptr;
        VulkanPipeline* pipeline = nullptr;

        UniformBuffer* transformBuffer = nullptr;
        TransformPack* transformPack = nullptr;
        int transformIndex = -1;

        PushConstantView pushConstantView;

        int subpassIndex = 0;

        RenderNode();
        RenderNode(UniformBuffer* transformBuffer, TransformPack* transformPack, int transformIndex);
        RenderNode(UniformBuffer* transformBuffer, std::vector<TransformPack>& transformPacks, int transformIndex);

        DrawCommand createDrawCommand(uint32_t frameIndex) const;

        template <typename T>
        inline void setPushConstantView(const T& data)
        {
            pushConstantView.set(data);
        }

        inline void setPushConstantView(PushConstantView view)
        {
            pushConstantView = view;
        }
    };
}
