#pragma once

#include <memory>

#include <vulkan/Vulkan.h>

namespace crisp
{
    class VulkanRenderer;
    class VulkanDevice;
    class VulkanImage;
    class VulkanImageView;
    class VulkanBuffer;

    class Texture
    {
    public:
        Texture(VulkanRenderer* renderer, VkExtent3D extent, uint32_t numLayers, VkFormat format,
                VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageCreateFlags createFlags = 0);
        Texture(VulkanRenderer* renderer, VkExtent3D extent, uint32_t numLayers, uint32_t numMipmaps, VkFormat format,
            VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageCreateFlags createFlags = 0);
        ~Texture();

        VulkanImage* getImage() const;

        void transitionLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout, uint32_t baseLayer, uint32_t numLayers, VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        void updateFromStaging(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, uint32_t baseLayer, uint32_t numLayers);
        void fill(const void* data, VkDeviceSize size);
        void fill(const void* data, VkDeviceSize size, uint32_t baseLayer, uint32_t numLayers);
        void fill(const VulkanBuffer& buffer, VkDeviceSize size);
        void fill(const VulkanBuffer& buffer, VkDeviceSize size, uint32_t baseLayer, uint32_t numLayers);

        std::unique_ptr<VulkanImageView> createView(VkImageViewType type, uint32_t baseLayer, uint32_t numLayers, uint32_t baseMipLevel = 0, uint32_t mipLevels = 1);

    private:
        VulkanRenderer* m_renderer;

        std::unique_ptr<VulkanImage> m_image;
    };
}