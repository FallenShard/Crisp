#pragma once

#include <Crisp/Math/Headers.hpp>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace crisp {
class AbstractCamera;

class Renderer;

class ShadowPass;
class VulkanPipeline;

class ShadowMapper {
public:
    ShadowMapper(Renderer* renderer, uint32_t numLights);
    ~ShadowMapper();

    void setLightTransform(uint32_t lightIndex, const glm::mat4& transform);
    void setLightFullTransform(uint32_t lightIndex, const glm::mat4& view, const glm::mat4& projection);

    VulkanRingBuffer* getLightTransformBuffer() const;
    VulkanRingBuffer* getLightFullTransformBuffer() const;

private:
    std::vector<glm::mat4> m_lightTransforms;
    std::unique_ptr<VulkanRingBuffer> m_lightTransformBuffer;

    std::vector<glm::mat4> m_lightFullTransforms;
    std::unique_ptr<VulkanRingBuffer> m_lightFullTransformBuffer;
};
} // namespace crisp