#pragma once

#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
class Renderer;

class ImageCache {
public:
    ImageCache(Renderer* renderer);

    void addImageWithView(
        const std::string& key,
        std::unique_ptr<VulkanImage> image,
        VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D);
    void addImageWithView(
        const std::string& key, std::unique_ptr<VulkanImage> image, std::unique_ptr<VulkanImageView> imageView);
    void addImage(const std::string& key, std::unique_ptr<VulkanImage> image);
    VulkanImage& getImage(const std::string& key) const;

    void removeImage(const std::string& key);
    void removeImageView(const std::string& key);

    void addImageView(const std::string& key, std::unique_ptr<VulkanImageView> imageView);
    VulkanImageView& getImageView(const std::string& key) const;
    VulkanImageView& getImageView(const std::string& key, const std::string& fallbackKey) const;

    void addSampler(const std::string& key, std::unique_ptr<VulkanSampler> sampler);
    VulkanSampler& getSampler(const std::string& key) const;

    inline Renderer* getRenderer() {
        return m_renderer;
    }

private:
    Renderer* m_renderer;
    robin_hood::unordered_flat_map<std::string, std::unique_ptr<VulkanImage>> m_images;
    robin_hood::unordered_flat_map<std::string, std::unique_ptr<VulkanImageView>> m_imageViews;
    robin_hood::unordered_flat_map<std::string, std::unique_ptr<VulkanSampler>> m_samplers;
};

} // namespace crisp