#pragma once

#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanImageView.hpp"

namespace crisp
{
    class Renderer;

    std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> convertEquirectToCubeMap(Renderer* renderer, std::shared_ptr<VulkanImageView> equirectMapView, uint32_t cubeMapSize);
    std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> setupDiffuseEnvMap(Renderer* renderer, const VulkanImageView& cubeMapView, uint32_t cubeMapSize);
    std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> setupReflectEnvMap(Renderer* renderer, const VulkanImageView& cubeMapView, uint32_t cubeMapSize);
    std::unique_ptr<VulkanImage> integrateBrdfLut(Renderer* renderer);
}
