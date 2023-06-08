#pragma once

#include <memory>
#include <vector>

#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <Crisp/PathTracer/RayTracerUpdate.hpp>

namespace crisp
{
class Renderer;
class VulkanDevice;
class VulkanBuffer;
class StagingVulkanBuffer;
class VulkanSampler;
class Material;
class VulkanImageView;
class VulkanPipeline;
class VulkanImage;

class RayTracedImage
{
public:
    RayTracedImage(uint32_t width, uint32_t height, Renderer* renderer);
    ~RayTracedImage();

    void postTextureUpdate(RayTracerUpdate rayTracerUpdate);

    void draw(Renderer* renderer);
    void resize(int width, int height);

private:
    std::unique_ptr<VulkanPipeline> m_pipeline;
    std::unique_ptr<Material> m_material;

    VkExtent3D m_extent;
    uint32_t m_numChannels;

    std::vector<std::pair<unsigned int, RayTracerUpdate>> m_textureUpdates;
    std::unique_ptr<StagingVulkanBuffer> m_stagingBuffer;

    std::unique_ptr<VulkanImage> m_image;
    std::vector<std::unique_ptr<VulkanImageView>> m_imageViews;
    std::unique_ptr<VulkanSampler> m_sampler;

    VkViewport m_viewport;
};
} // namespace crisp