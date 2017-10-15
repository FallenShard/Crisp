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

    class VulkanRenderer;
    class UniformBuffer;

    class ShadowPass;
    class VulkanPipeline;
    class ShadowMapPipeline;

    enum class ParallelSplit
    {
        Linear,
        Logarithmic
    };

    class CascadedShadowMapper
    {
    public:
        CascadedShadowMapper(VulkanRenderer* renderer, DirectionalLight light, uint32_t numCascades, UniformBuffer* modelTransformsBuffer);
        ~CascadedShadowMapper();

        UniformBuffer* getLightTransformsBuffer() const;
        ShadowPass* getRenderPass() const;

        void recalculateLightProjections(const AbstractCamera& camera, float zFar, ParallelSplit splitStrategy);

        void update(VkCommandBuffer cmdBuffer, unsigned int frameIndex);
        void draw(VkCommandBuffer cmdBuffer, unsigned int frameIndex, std::function<void(VkCommandBuffer commandBuffer, uint32_t frameIdx, DescriptorSetGroup& descSets, VulkanPipeline* pipeline, uint32_t cascadeTransformOffset)> callback);
       
        glm::mat4 getLightTransform(uint32_t index) const;

        const DirectionalLight* getLight() const;

    private:
        VulkanRenderer* m_renderer;

        uint32_t m_numCascades;
        DirectionalLight m_light;

        std::unique_ptr<ShadowPass> m_shadowPass;
        std::vector<std::unique_ptr<ShadowMapPipeline>> m_pipelines;
        DescriptorSetGroup m_descGroup;

        std::vector<glm::mat4> m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;
    };
}