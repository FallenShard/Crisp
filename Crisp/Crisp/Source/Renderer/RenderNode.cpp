#include "RenderNode.hpp"

namespace crisp
{
    RenderNode::RenderNode()
    {
    }

    RenderNode::RenderNode(UniformBuffer* transformBuffer, TransformPack* transformPack, int transformIndex)
        : transformBuffer(transformBuffer)
        , transformPack(transformPack)
        , transformIndex(transformIndex)
    {
    }

    RenderNode::RenderNode(UniformBuffer* transformBuffer, std::vector<TransformPack>& transformPacks, int transformIndex)
        : RenderNode(transformBuffer, &transformPacks[transformIndex], transformIndex)
    {
    }

    DrawCommand RenderNode::createDrawCommand(uint32_t frameIndex) const
    {
        DrawCommand drawCommand;
        drawCommand.pipeline = pipeline;
        drawCommand.material = material;
        drawCommand.dynamicBufferViews = material->getDynamicBufferViews();

        if (transformBuffer)
            drawCommand.dynamicBufferViews[0] = { transformBuffer, transformIndex * sizeof(TransformPack) };

        drawCommand.dynamicBufferOffsets.resize(drawCommand.dynamicBufferViews.size());
        for (std::size_t i = 0; i < drawCommand.dynamicBufferOffsets.size(); ++i)
            drawCommand.dynamicBufferOffsets[i] = drawCommand.dynamicBufferViews[i].buffer->getDynamicOffset(frameIndex) + drawCommand.dynamicBufferViews[i].subOffset;
        drawCommand.geometry = geometry;
        drawCommand.setPushConstantView(pushConstantView);
        drawCommand.setGeometryView(geometry->createIndexedGeometryView());
        return drawCommand;
    }
}