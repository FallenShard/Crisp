#pragma once

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>

#include <CrispCore/IO/ImageLoader.hpp>
#include <CrispCore/Image/Image.hpp>

namespace crisp
{
void fillLayer(VulkanImage& image, Renderer* renderer, const void* data, VkDeviceSize size, uint32_t layerIdx);
void fillLayers(VulkanImage& image, Renderer* renderer, const void* data, VkDeviceSize size, uint32_t layerIdx,
    uint32_t numLayers);

std::unique_ptr<VulkanImage> createTexture(Renderer* renderer, const Image& image, VkFormat format);
std::unique_ptr<VulkanImage> createTexture(Renderer* renderer, const std::string& filename, VkFormat format,
    FlipOnLoad flipOnLoad = FlipOnLoad::None);
std::unique_ptr<VulkanImage> createTexture(Renderer* renderer, VkDeviceSize size, const void* data,
    VkImageCreateInfo imageCreateInfo, VkImageAspectFlags imageAspect);

std::unique_ptr<VulkanImage> createEnvironmentMap(Renderer* renderer, const std::string& filename, VkFormat format,
    FlipOnLoad flipOnLoad = FlipOnLoad::None);
std::unique_ptr<VulkanImage> createMipmapCubeMap(Renderer* renderer, uint32_t w, uint32_t h, uint32_t mipLevels);
} // namespace crisp