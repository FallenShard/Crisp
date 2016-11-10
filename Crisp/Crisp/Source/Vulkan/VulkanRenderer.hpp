#pragma once

#include <memory>

#include <vulkan/vulkan.h>

#include "VulkanContext.hpp"
#include "VulkanDevice.hpp"

namespace crisp
{
    class VulkanRenderer
    {
    public:
        VulkanRenderer(SurfaceCreator surfCreatorCallback, std::vector<const char*>&& extensions);
        ~VulkanRenderer() = default;

        VulkanRenderer(const VulkanRenderer& other) = delete;
        VulkanRenderer& operator=(const VulkanRenderer& other) = delete;
        VulkanRenderer& operator=(VulkanRenderer&& other) = delete;

    private:
        std::unique_ptr<VulkanContext> m_context;
        std::unique_ptr<VulkanDevice>  m_device;
    };
}