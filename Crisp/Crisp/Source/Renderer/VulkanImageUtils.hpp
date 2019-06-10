#pragma once

#include "Renderer.hpp"
#include "Vulkan/VulkanImage.hpp"

#include "IO/ImageFileBuffer.hpp"

namespace crisp
{
    void fillLayer(VulkanImage& image, Renderer* renderer, const void* data, VkDeviceSize size, uint32_t layerIdx);
    void fillLayers(VulkanImage& image, Renderer* renderer, const void* data, VkDeviceSize size, uint32_t layerIdx, uint32_t numLayers);
    std::unique_ptr<VulkanImage> createTexture(Renderer* renderer, uint32_t width, uint32_t height, const std::vector<unsigned char>& data, VkFormat format);
    std::unique_ptr<VulkanImage> createTexture(Renderer* renderer, const ImageFileBuffer& imageFileBuffer, VkFormat format);
    std::unique_ptr<VulkanImage> createTexture(Renderer* renderer, const std::string& filename, VkFormat format, bool invertY = false);
    std::unique_ptr<VulkanImage> createTexture(Renderer* renderer, VkDeviceSize size, const void* data, VkImageCreateInfo imageCreateInfo, VkImageAspectFlags imageAspect);
    std::unique_ptr<VulkanImage> createEnvironmentMap(Renderer* renderer, const std::string& filename, VkFormat format, bool invertY = false);
    std::unique_ptr<VulkanImage> createMipmapCubeMap(Renderer* renderer, uint32_t w, uint32_t h, uint32_t mipLevels);
}