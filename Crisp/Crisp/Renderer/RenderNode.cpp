#include <Crisp/Renderer/RenderNode.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>

namespace crisp {

RenderNode::RenderNode(VulkanRingBuffer* transformBuffer, TransformPack* transformPack, TransformHandle transformHandle)
    : transformBuffer(transformBuffer)
    , transformPack(transformPack)
    , transformHandle(transformHandle) {}

RenderNode::RenderNode(
    VulkanRingBuffer* transformBuffer, std::vector<TransformPack>& transformPacks, TransformHandle transformHandle)
    : RenderNode(transformBuffer, &transformPacks[transformHandle.index], transformHandle) {}

RenderNode::RenderNode(TransformBuffer& transformBuffer, TransformHandle transformHandle)
    : RenderNode(transformBuffer.getUniformBuffer(), &transformBuffer.getPack(transformHandle), transformHandle) {}

DrawCommand RenderNode::MaterialData::createDrawCommand(const RenderNode& renderNode) const {
    DrawCommand drawCommand;
    drawCommand.pipeline = pipeline ? pipeline : material->getPipeline();
    drawCommand.material = material;

    drawCommand.dynamicBufferOffsets.resize(drawCommand.material->getDynamicDescriptorCount());
    CRISP_CHECK_GE_LT(transformBufferDynamicIndex, 0, drawCommand.dynamicBufferOffsets.size());
    drawCommand.dynamicBufferOffsets[transformBufferDynamicIndex] =
        renderNode.transformHandle.index * sizeof(TransformPack);

    drawCommand.setPushConstantView(pushConstantView);

    drawCommand.geometry = geometry ? geometry : renderNode.geometry;
    if (!drawCommand.geometry->getIndexBuffer()) {
        drawCommand.setGeometryView(drawCommand.geometry->createListGeometryView());
    } else if (part != -1) {
        drawCommand.setGeometryView(drawCommand.geometry->createIndexedGeometryView(part));
    } else {
        drawCommand.setGeometryView(drawCommand.geometry->createIndexedGeometryView());
    }
    drawCommand.firstBuffer = firstBuffer == -1 ? 0 : firstBuffer;
    drawCommand.bufferCount = bufferCount == -1 ? drawCommand.geometry->getVertexBufferCount() : bufferCount;

    CRISP_CHECK(drawCommand.pipeline->getVertexLayout().bindings.size() == drawCommand.bufferCount);

    return drawCommand;
}
} // namespace crisp