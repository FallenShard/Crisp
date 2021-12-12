#pragma once

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanDevice;
    class VulkanCommandPool;

    class VulkanAccelerationStructure
    {
    public:
        explicit VulkanAccelerationStructure(VkAccelerationStructureKHR accelerationStructure);
        ~VulkanAccelerationStructure();
        
    private:
        VkAccelerationStructureKHR m_handle;
    };
}