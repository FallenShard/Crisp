#include "RenderNode.hpp"

#include <Crisp/Common/Checks.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>

namespace crisp
{
RenderNode::RenderNode() {}

RenderNode::RenderNode(UniformBuffer* transformBuffer, TransformPack* transformPack, TransformHandle transformHandle)
    : transformBuffer(transformBuffer)
    , transformPack(transformPack)
    , transformHandle(transformHandle)
{
}

RenderNode::RenderNode(
    UniformBuffer* transformBuffer, std::vector<TransformPack>& transformPacks, TransformHandle transformHandle)
    : RenderNode(transformBuffer, &transformPacks[transformHandle.index], transformHandle)
{
}

RenderNode::RenderNode(TransformBuffer& transformBuffer, TransformHandle transformHandle)
    : RenderNode(transformBuffer.getUniformBuffer(), &transformBuffer.getPack(transformHandle), transformHandle)
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
        drawCommand.dynamicBufferViews[0] = {
            renderNode.transformBuffer,
            static_cast<uint32_t>(renderNode.transformHandle.index * sizeof(TransformPack))};

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
    drawCommand.firstBuffer = firstBuffer == -1 ? 0 : firstBuffer;
    drawCommand.bufferCount = bufferCount == -1 ? drawCommand.geometry->getVertexBufferCount() : bufferCount;

    CRISP_CHECK(drawCommand.pipeline->getVertexLayout().bindings.size() == drawCommand.bufferCount);

    return drawCommand;
}
} // namespace crisp