#pragma once

#include <CrispCore/Math/Headers.hpp>

#include <vulkan/vulkan.h>

#include <vector>
#include <memory>
#include <functional>

namespace crisp
{
    class AbstractCamera;

    class Renderer;
    class UniformBuffer;

    class ShadowPass;
    class VulkanPipeline;

    class ShadowMapper
    {
    public:
        ShadowMapper(Renderer* renderer, uint32_t numLights);
        ~ShadowMapper();

        void setLightTransform(uint32_t lightIndex, const glm::mat4& transform);
        void setLightFullTransform(uint32_t lightIndex, const glm::mat4& view, const glm::mat4& projection);

        UniformBuffer* getLightTransformBuffer() const;
        UniformBuffer* getLightFullTransformBuffer() const;

    private:
        std::vector<glm::mat4>         m_lightTransforms;
        std::unique_ptr<UniformBuffer> m_lightTransformBuffer;

        std::vector<glm::mat4>         m_lightFullTransforms;
        std::unique_ptr<UniformBuffer> m_lightFullTransformBuffer;
    };
}