#include "RenderNode.hpp"

namespace crisp
{
RenderNode::RenderNode() {}

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

RenderNode::RenderNode(TransformBuffer& transformBuffer, int transformIndex)
    : RenderNode(transformBuffer.getUniformBuffer(), transformBuffer.getPack(transformIndex), transformIndex)
{
}

DrawCommand RenderNode::MaterialData::createDrawCommand(uint32_t frameIndex, const RenderNode& renderNode) const
{
    DrawCommand drawCommand;
    drawCommand.pipeline = pipeline ? pipeline : material->getPipeline();
    drawCommand.material = material;

    if (material)
        drawCommand.dynamicBufferViews = material->getDynamicBufferViews();

    if (renderNode.transformBuffer)
        drawCommand.dynamicBufferViews[0] = { renderNode.transformBuffer,
            renderNode.transformIndex * sizeof(TransformPack) };

    drawCommand.dynamicBufferOffsets.resize(drawCommand.dynamicBufferViews.size());
    for (std::size_t i = 0; i < drawCommand.dynamicBufferOffsets.size(); ++i)
        drawCommand.dynamicBufferOffsets[i] = drawCommand.dynamicBufferViews[i].buffer->getDynamicOffset(frameIndex) +
                                              drawCommand.dynamicBufferViews[i].subOffset;

    drawCommand.setPushConstantView(pushConstantView);

    drawCommand.geometry = geometry ? geometry : renderNode.geometry;
    if (!drawCommand.geometry->getIndexBuffer())
        drawCommand.setGeometryView(drawCommand.geometry->createListGeometryView());
    else if (part != -1)
        drawCommand.setGeometryView(drawCommand.geometry->createIndexedGeometryView(part));
    else
        drawCommand.setGeometryView(drawCommand.geometry->createIndexedGeometryView());

    return drawCommand;
}
} // namespace crisp