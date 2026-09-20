#include <Crisp/Renderer/RenderNode.hpp>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>

namespace crisp {
namespace {
struct RenderPassRegistry {
    std::mutex mutex;
    FlatStringHashMap<RenderPassId> idsByName;
    std::vector<std::string> namesById;
};

RenderPassRegistry& getRenderPassRegistry() {
    static RenderPassRegistry registry;
    return registry;
}
} // namespace

RenderPassId internRenderPassId(const std::string_view renderPassName) {
    auto& registry = getRenderPassRegistry();
    const std::scoped_lock lock(registry.mutex);
    if (const auto it = registry.idsByName.find(renderPassName); it != registry.idsByName.end()) {
        return it->second;
    }

    CRISP_CHECK_LT(registry.namesById.size(), kInvalidRenderPassId, "Ran out of render pass ids.");
    const auto passId = static_cast<RenderPassId>(registry.namesById.size());
    registry.namesById.emplace_back(renderPassName);
    registry.idsByName.emplace(std::string(renderPassName), passId);
    return passId;
}

std::string_view getRenderPassName(const RenderPassId passId) {
    auto& registry = getRenderPassRegistry();
    const std::scoped_lock lock(registry.mutex);
    return passId < registry.namesById.size() ? std::string_view{registry.namesById[passId]} : std::string_view{};
}

RenderNode::MaterialData& RenderNode::findOrAddMaterial(const RenderPassId passId) {
    const auto it = std::ranges::find_if(materials, [=](const MaterialData& entry) { return entry.passId == passId; });
    if (it != materials.end()) {
        return *it;
    }

    materials.push_back({.passId = passId});
    return materials.back();
}

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
    drawCommand.material = material;

    drawCommand.dynamicBufferOffsetCount =
        static_cast<uint8_t>(drawCommand.material->getDynamicDescriptorCount());
    CRISP_CHECK_LE(drawCommand.dynamicBufferOffsetCount, DrawCommand::kMaxDynamicBufferOffsets);
    if (drawCommand.dynamicBufferOffsetCount > 0) {
        CRISP_CHECK_GE_LT(transformBufferDynamicIndex, 0, drawCommand.dynamicBufferOffsetCount);
        drawCommand.dynamicBufferOffsets[transformBufferDynamicIndex] =
            renderNode.transformHandle.index * sizeof(TransformPack);
    }

    PushConstantView ownedPushConstantView;
    ownedPushConstantView.data = pushConstantBuffer.data();
    ownedPushConstantView.size = pushConstantSize;
    drawCommand.setPushConstantView(ownedPushConstantView);

    drawCommand.geometry = geometry ? geometry : renderNode.geometry;
    if (!drawCommand.geometry->getIndexBuffer()) {
        drawCommand.geometryView = drawCommand.geometry->createListGeometryView();
    } else if (renderNode.geometryPartIndex >= 0) {
        drawCommand.geometryView =
            drawCommand.geometry->createIndexedGeometryView(static_cast<uint32_t>(renderNode.geometryPartIndex));
    } else {
        drawCommand.geometryView = drawCommand.geometry->createIndexedGeometryView();
    }
    drawCommand.firstBuffer = firstBuffer == -1 ? 0 : static_cast<uint8_t>(firstBuffer);
    drawCommand.bufferCount = bufferCount == -1
                                  ? static_cast<uint8_t>(drawCommand.geometry->getVertexBufferCount())
                                  : static_cast<uint8_t>(bufferCount);

    CRISP_CHECK(drawCommand.getPipeline()->getVertexLayout().bindings.size() == drawCommand.bufferCount);

    return drawCommand;
}
} // namespace crisp
