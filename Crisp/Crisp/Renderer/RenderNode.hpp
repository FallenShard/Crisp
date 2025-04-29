#pragma once

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Renderer/DrawCommand.hpp>
#include <Crisp/Renderer/Material.hpp>

#include <Crisp/Core/HashMap.hpp>

namespace crisp {
struct RenderNode {
    void setModelMatrix(const glm::mat4& mat) const {
        transformPack->M = mat;
    }

    struct SubpassKey {
        std::string renderPassName;
        int subpassIndex = 0;

        bool operator==(const SubpassKey& rhs) const {
            return renderPassName == rhs.renderPassName && subpassIndex == rhs.subpassIndex;
        }
    };

    struct SubpassKeyHasher {
        std::size_t operator()(const SubpassKey& key) const {
            return std::hash<std::string>()(key.renderPassName) ^ std::hash<int>()(key.subpassIndex);
        }
    };

    struct MaterialData {
        int part = -1;

        Geometry* geometry = nullptr;
        int firstBuffer = -1;
        int bufferCount = -1;
        Material* material = nullptr;
        VulkanPipeline* pipeline = nullptr;
        uint32_t transformBufferDynamicIndex = 0;

        std::vector<unsigned char> pushConstantBuffer;
        PushConstantView pushConstantView;

        void setGeometry(Geometry* newGeometry, int firstVertexBuffer, int vertexBufferCount) {
            geometry = newGeometry;
            firstBuffer = firstVertexBuffer;
            bufferCount = vertexBufferCount;
        }

        template <typename T>
        void setPushConstantView(const T& data) {
            pushConstantView.set(data);
        }

        template <typename T>
        void setPushConstants(const T& data) {
            pushConstantBuffer.resize(sizeof(T));
            std::memcpy(pushConstantBuffer.data(), &data, sizeof(T));
            pushConstantView.set(pushConstantBuffer);
        }

        void setPushConstantView(PushConstantView view) {
            pushConstantView = view;
        }

        DrawCommand createDrawCommand(const RenderNode& renderNode) const;
    };

    MaterialData& pass(std::string renderPassName) {
        return materials[SubpassKey{std::move(renderPassName), 0}][-1];
    }

    MaterialData& subpass(std::string renderPassName, int subpassIndex) {
        return materials[SubpassKey{std::move(renderPassName), subpassIndex}][-1];
    }

    MaterialData& pass(int part, std::string renderPassName) {
        MaterialData& matData = materials[SubpassKey{std::move(renderPassName), 0}][part];
        matData.part = part;
        return matData;
    }

    MaterialData& subpass(int part, std::string renderPassName, int subpassIndex) {
        MaterialData& matData = materials[SubpassKey{std::move(renderPassName), subpassIndex}][part];
        matData.part = part;
        return matData;
    }

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
    FlatHashMap<SubpassKey, FlatHashMap<int32_t, MaterialData>, SubpassKeyHasher> materials;
};

struct RenderNodeLite {
    Geometry* geometry = nullptr;
    VulkanRingBuffer* transformBuffer = nullptr;
    TransformPack* transformPack = nullptr;
    TransformHandle transformHandle{TransformHandle::createInvalidHandle()};
    bool isVisible = true;
    Material* material = nullptr;

    // void setModelMatrix(const glm::mat4& mat) const {
    //     transformPack->M = mat;
    // }

    // struct SubpassKey {
    //     std::string renderPassName;
    //     int subpassIndex = 0;

    //     bool operator==(const SubpassKey& rhs) const {
    //         return renderPassName == rhs.renderPassName && subpassIndex == rhs.subpassIndex;
    //     }
    // };

    // struct SubpassKeyHasher {
    //     std::size_t operator()(const SubpassKey& key) const {
    //         return std::hash<std::string>()(key.renderPassName) ^ std::hash<int>()(key.subpassIndex);
    //     }
    // };

    // struct MaterialData {
    //     int part = -1;

    //     Geometry* geometry = nullptr;
    //     int firstBuffer = -1;
    //     int bufferCount = -1;
    //     Material* material = nullptr;
    //     VulkanPipeline* pipeline = nullptr;
    //     uint32_t transformBufferDynamicIndex = 0;

    //     std::vector<unsigned char> pushConstantBuffer;
    //     PushConstantView pushConstantView;

    //     void setGeometry(Geometry* newGeometry, int firstVertexBuffer, int vertexBufferCount) {
    //         geometry = newGeometry;
    //         firstBuffer = firstVertexBuffer;
    //         bufferCount = vertexBufferCount;
    //     }

    //     template <typename T>
    //     void setPushConstantView(const T& data) {
    //         pushConstantView.set(data);
    //     }

    //     template <typename T>
    //     void setPushConstants(const T& data) {
    //         pushConstantBuffer.resize(sizeof(T));
    //         std::memcpy(pushConstantBuffer.data(), &data, sizeof(T));
    //         pushConstantView.set(pushConstantBuffer);
    //     }

    //     void setPushConstantView(PushConstantView view) {
    //         pushConstantView = view;
    //     }

    //     DrawCommand createDrawCommand(const RenderNode& renderNode) const;
    // };

    // MaterialData& pass(std::string renderPassName) {
    //     return materials[SubpassKey{std::move(renderPassName), 0}][-1];
    // }

    // MaterialData& subpass(std::string renderPassName, int subpassIndex) {
    //     return materials[SubpassKey{std::move(renderPassName), subpassIndex}][-1];
    // }

    // MaterialData& pass(int part, std::string renderPassName) {
    //     MaterialData& matData = materials[SubpassKey{std::move(renderPassName), 0}][part];
    //     matData.part = part;
    //     return matData;
    // }

    // MaterialData& subpass(int part, std::string renderPassName, int subpassIndex) {
    //     MaterialData& matData = materials[SubpassKey{std::move(renderPassName), subpassIndex}][part];
    //     matData.part = part;
    //     return matData;
    // }

    // RenderNode() = default;
    // RenderNode(VulkanRingBuffer* transformBuffer, TransformPack* transformPack, TransformHandle transformHandle);
    // RenderNode(
    //     VulkanRingBuffer* transformBuffer, std::vector<TransformPack>& transformPacks, TransformHandle
    //     transformHandle);
    // RenderNode(TransformBuffer& transformBuffer, TransformHandle transformHandle);

    // Geometry* geometry = nullptr;
    // VulkanRingBuffer* transformBuffer = nullptr;
    // TransformPack* transformPack = nullptr;
    // TransformHandle transformHandle{TransformHandle::createInvalidHandle()};
    // bool isVisible = true;
    // FlatHashMap<SubpassKey, FlatHashMap<int32_t, MaterialData>, SubpassKeyHasher> materials;
};
} // namespace crisp
