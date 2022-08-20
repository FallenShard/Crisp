#include <Crisp/Renderer/ImageCache.hpp>

#include "ImageCache.hpp"
#include <Crisp/Renderer/Renderer.hpp>

namespace crisp
{

ImageCache::ImageCache(Renderer* renderer)
    : m_renderer{renderer}
{
}

void ImageCache::addImageWithView(
    const std::string& key, std::unique_ptr<VulkanImage> image, VkImageViewType imageViewType)
{
    m_imageViews[key] = image->createView(imageViewType);
    m_images[key] = std::move(image);
}

void ImageCache::addImageWithView(
    const std::string& key, std::unique_ptr<VulkanImage> image, std::unique_ptr<VulkanImageView> imageView)
{
    m_imageViews[key] = std::move(imageView);
    m_images[key] = std::move(image);
}

void ImageCache::addImage(const std::string& key, std::unique_ptr<VulkanImage> image)
{
    m_images[key] = std::move(image);
}

VulkanImage& ImageCache::getImage(const std::string& key) const
{
    return *m_images.at(key);
}

void ImageCache::addImageView(const std::string& key, std::unique_ptr<VulkanImageView> imageView)
{
    m_imageViews[key] = std::move(imageView);
}

VulkanImageView& ImageCache::getImageView(const std::string& key) const
{
    return *m_imageViews.at(key);
}

VulkanImageView& ImageCache::getImageView(const std::string& key, const std::string& fallbackKey) const
{
    auto found = m_imageViews.find(key);
    if (found != m_imageViews.end())
    {
        return *found->second;
    }
    else
    {
        return *m_imageViews.at(fallbackKey);
    }
}

void ImageCache::addSampler(const std::string& key, std::unique_ptr<VulkanSampler> sampler)
{
    m_samplers[key] = std::move(sampler);
}

VulkanSampler& ImageCache::getSampler(const std::string& key) const
{
    return *m_samplers.at(key);
}

} // namespace crisp