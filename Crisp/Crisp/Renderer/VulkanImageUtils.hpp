#pragma once

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>

#include <Crisp/IO/ImageLoader.hpp>
#include <Crisp/Image/Image.hpp>

namespace crisp
{
void fillImageLayer(VulkanImage& image, Renderer& renderer, const void* data, VkDeviceSize size, uint32_t layerIdx);
void fillImageLayers(
    VulkanImage& image, Renderer& renderer, const void* data, VkDeviceSize size, uint32_t layerIdx, uint32_t numLayers);

std::unique_ptr<VulkanImage> convertToVulkanImage(Renderer* renderer, const Image& image, VkFormat format);

std::unique_ptr<VulkanImage> createVulkanImage(
    Renderer& renderer, VkDeviceSize size, const void* data, VkImageCreateInfo imageCreateInfo);

std::unique_ptr<VulkanImage> createEnvironmentMap(
    Renderer* renderer, const std::string& filename, VkFormat format, FlipOnLoad flipOnLoad = FlipOnLoad::None);
std::unique_ptr<VulkanImage> createMipmapCubeMap(Renderer* renderer, uint32_t w, uint32_t h, uint32_t mipLevels);

std::unique_ptr<VulkanImage> createSampledStorageImage(const Renderer& renderer, VkFormat format, VkExtent3D extent);

void transitionComputeWriteToFragmentShading();

} // namespace crisp