#pragma once

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Renderer/DrawCommand.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>

#include <Crisp/Common/HashMap.hpp>

namespace crisp
{
struct RenderNode
{

    inline void setModelMatrix(const glm::mat4& mat)
    {
        transformPack->M = mat;
    }

    struct SubpassKey
    {
        std::string renderPassName;
        int subpassIndex = 0;

        inline bool operator==(const SubpassKey& rhs) const
        {
            return renderPassName == rhs.renderPassName && subpassIndex == rhs.subpassIndex;
        }
    };

    struct SubpassKeyHasher
    {
        inline std::size_t operator()(const SubpassKey& key) const
        {
            return std::hash<std::string>()(key.renderPassName) ^ std::hash<int>()(key.subpassIndex);
        }
    };

    struct MaterialData
    {
        int part = -1;

        Geometry* geometry = nullptr;
        int firstBuffer = -1;
        int bufferCount = -1;
        Material* material = nullptr;
        VulkanPipeline* pipeline = nullptr;

        std::vector<unsigned char> pushConstantBuffer;
        PushConstantView pushConstantView;

        inline void setGeometry(Geometry* newGeometry, int firstVertexBuffer, int vertexBufferCount)
        {
            geometry = newGeometry;
            firstBuffer = firstVertexBuffer;
            bufferCount = vertexBufferCount;
        }

        template <typename T>
        inline void setPushConstantView(const T& data)
        {
            pushConstantView.set(data);
        }

        template <typename T>
        inline void setPushConstants(const T& data)
        {
            pushConstantBuffer.resize(sizeof(T));
            std::memcpy(pushConstantBuffer.data(), &data, sizeof(T));
            pushConstantView.set(pushConstantBuffer);
        }

        inline void setPushConstantView(PushConstantView view)
        {
            pushConstantView = view;
        }

        DrawCommand createDrawCommand(uint32_t frameIndex, const RenderNode& renderNode) const;
    };

    MaterialData& pass(std::string renderPassName)
    {
        return materials[SubpassKey{renderPassName, 0}][-1];
    }

    MaterialData& subpass(std::string renderPassName, int subpassIndex)
    {
        return materials[SubpassKey{renderPassName, subpassIndex}][-1];
    }

    MaterialData& pass(int part, std::string renderPassName)
    {
        MaterialData& matData = materials[SubpassKey{renderPassName, 0}][part];
        matData.part = part;
        return matData;
    }

    MaterialData& subpass(int part, std::string renderPassName, int subpassIndex)
    {
        MaterialData& matData = materials[SubpassKey{renderPassName, subpassIndex}][part];
        matData.part = part;
        return matData;
    }

    RenderNode();
    RenderNode(UniformBuffer* transformBuffer, TransformPack* transformPack, TransformHandle transformHandle);
    RenderNode(
        UniformBuffer* transformBuffer, std::vector<TransformPack>& transformPacks, TransformHandle transformHandle);
    RenderNode(TransformBuffer& transformBuffer, TransformHandle transformHandle);

    Geometry* geometry = nullptr;
    UniformBuffer* transformBuffer = nullptr;
    TransformPack* transformPack = nullptr;
    TransformHandle transformHandle{TransformHandle::createInvalidHandle()};
    bool isVisible = true;
    FlatHashMap<SubpassKey, FlatHashMap<int32_t, MaterialData>, SubpassKeyHasher> materials;
};
} // namespace crisp
