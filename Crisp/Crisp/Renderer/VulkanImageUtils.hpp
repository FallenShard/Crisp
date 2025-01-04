#pragma once

#include <span>

#include <Crisp/Image/Image.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>

namespace crisp {
void fillImageLayer(VulkanImage& image, Renderer& renderer, const void* data, VkDeviceSize size, uint32_t layerIdx);
void fillImageLayers(
    VulkanImage& image, Renderer& renderer, const void* data, VkDeviceSize size, uint32_t layerIdx, uint32_t numLayers);

std::unique_ptr<VulkanImage> createVulkanImage(
    Renderer& renderer, VkDeviceSize size, const void* data, VkImageCreateInfo imageCreateInfo);
std::unique_ptr<VulkanImage> createVulkanImage(Renderer& renderer, const Image& image, VkFormat format);
std::unique_ptr<VulkanImage> createVulkanCubeMap(
    Renderer& renderer, std::span<const std::vector<Image>> cubeMapFaceMips, VkFormat format);

void updateCubeMap(
    VulkanImage& cubeMap, Renderer& renderer, const std::vector<Image>& cubeMapFaces, uint32_t mipLevel = 0);

std::unique_ptr<VulkanImage> createMipmapCubeMap(Renderer* renderer, uint32_t w, uint32_t h, uint32_t mipLevels);

std::unique_ptr<VulkanImage> createSampledStorageImage(const Renderer& renderer, VkFormat format, VkExtent3D extent);
std::unique_ptr<VulkanImage> createStorageImage(
    VulkanDevice& device, uint32_t layerCount, uint32_t width, uint32_t height, VkFormat format);
} // namespace crisp