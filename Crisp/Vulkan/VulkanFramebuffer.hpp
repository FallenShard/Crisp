#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include "VulkanResource.hpp"

namespace crisp
{
    class VulkanDevice;

    class VulkanFramebuffer : public VulkanResource<VkFramebuffer, vkDestroyFramebuffer>
    {
    public:
        VulkanFramebuffer(VulkanDevice* device, VkRenderPass renderPass, VkExtent2D extent, const std::vector<VkImageView>& attachmentList);
    };

}