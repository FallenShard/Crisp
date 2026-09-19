#pragma once

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Renderer/DrawCommand.hpp>
#include <Crisp/Renderer/Material.hpp>

#include <Crisp/Core/HashMap.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace crisp {

using RenderPassId = uint16_t;
inline constexpr RenderPassId kInvalidRenderPassId{static_cast<RenderPassId>(~0u)};

RenderPassId internRenderPassId(std::string_view renderPassName);
std::string_view getRenderPassName(RenderPassId passId);

struct RenderNode {
    void setModelMatrix(const glm::mat4& mat) const {
        transformPack->M = mat;
    }

    struct MaterialData {
        // Vulkan guarantees at least 128 bytes of push-constant storage on every device.
        static constexpr size_t kMaxPushConstantBytes{128};

        Geometry* geometry = nullptr;
        Material* material = nullptr;
        RenderPassId passId{kInvalidRenderPassId};
        int16_t firstBuffer = -1;
        int16_t bufferCount = -1;
        uint8_t transformBufferDynamicIndex = 0;
        uint8_t pushConstantSize = 0;

        std::array<std::byte, kMaxPushConstantBytes> pushConstantBuffer{};

        void setGeometry(Geometry* newGeometry, const int firstVertexBuffer, const int vertexBufferCount) {
            geometry = newGeometry;
            firstBuffer = static_cast<int16_t>(firstVertexBuffer);
            bufferCount = static_cast<int16_t>(vertexBufferCount);
        }

        template <typename T>
        void setPushConstants(const T& data) {
            static_assert(std::is_trivially_copyable_v<T>);
            static_assert(sizeof(T) <= kMaxPushConstantBytes, "Push constants exceed inline storage capacity.");
            std::memcpy(pushConstantBuffer.data(), &data, sizeof(T));
            pushConstantSize = static_cast<uint8_t>(sizeof(T));
        }

        DrawCommand createDrawCommand(const RenderNode& renderNode) const;
    };

    MaterialData& pass(const std::string_view renderPassName) {
        return findOrAddMaterial(internRenderPassId(renderPassName));
    }

    MaterialData& findOrAddMaterial(RenderPassId passId);

    RenderNode() = default;
    RenderNode(VulkanRingBuffer* transformBuffer, TransformPack* transformPack, TransformHandle transformHandle);
    RenderNode(
        VulkanRingBuffer* transformBuffer, std::vector<TransformPack>& transformPacks, TransformHandle transformHandle);
    RenderNode(TransformBuffer& transformBuffer, TransformHandle transformHandle);

    Geometry* geometry = nullptr;
    VulkanRingBuffer* transformBuffer = nullptr;
    TransformPack* transformPack = nullptr;
    TransformHandle transformHandle{TransformHandle::createInvalidHandle()};
    bool isVisible = true;
    std::vector<MaterialData> materials;
};

} // namespace crisp
