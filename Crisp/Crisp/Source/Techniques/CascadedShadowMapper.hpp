#pragma once

#include <vector>
#include <memory>
#include <functional>

#include <vulkan/vulkan.h>

#include "Renderer/DescriptorSetGroup.hpp"
#include "Lights/DirectionalLight.hpp"

namespace crisp
{
    class AbstractCamera;

    class Renderer;
    class UniformBuffer;

    class ShadowPass;
    class VulkanPipeline;

    enum class SplitStrategy
    {
        Linear,
        Logarithmic,
        Hybrid
    };

    class CascadedShadowMapper
    {
    public:
        CascadedShadowMapper(Renderer* renderer, uint32_t numCascades, DirectionalLight light, float zFar);
        ~CascadedShadowMapper();

        UniformBuffer* getLightTransformBuffer() const;
        UniformBuffer* getSplitBuffer() const;

        glm::mat4 getLightTransform(uint32_t cascadeIndex) const;

        void setSplitLambda(float lambda, const AbstractCamera& camera);

        void recalculateLightProjections(const AbstractCamera& camera);

        void setZ(float z) { m_z = z; }

    private:
        uint32_t m_numCascades;
        DirectionalLight m_light;

        float m_zFar;

        std::vector<glm::mat4> m_lightTransforms;
        std::unique_ptr<UniformBuffer> m_lightTransformBuffer;

        float m_lambda; // Balances between linear and logarithmic split
        struct SplitIntervals
        {
            glm::vec4 lo;
            glm::vec4 hi;
        };
        SplitIntervals m_splitIntervals;
        std::unique_ptr<UniformBuffer> m_splitBuffer;

        float m_z;
    };
}