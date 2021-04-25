#pragma once

#include <vector>

#include "TransformPack.hpp"
#include "Renderer/UniformBuffer.hpp"

#include <Core/ThreadPool.hpp>

namespace crisp
{
    class TransformBuffer
    {
    public:
        TransformBuffer(Renderer* renderer, std::size_t numObjects);

        const UniformBuffer* getUniformBuffer() const;
        UniformBuffer* getUniformBuffer();

        TransformPack* getPack(int index);

        inline VkDescriptorBufferInfo getDescriptorInfo() const
        {
            return m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack));
        }

        void update(const glm::mat4& V, const glm::mat4& P);


    private:
        std::vector<TransformPack>     m_transforms;
        std::unique_ptr<UniformBuffer> m_transformBuffer;
        ThreadPool m_threadPool;
    };
}