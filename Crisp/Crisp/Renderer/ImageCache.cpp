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
    auto& cachedView = m_imageViews[key];
    if (!cachedView)
    {
        cachedView = createView(*image, imageViewType);
    }

    auto& cachedImage = m_images[key];
    if (!cachedImage)
    {
        cachedImage = std::move(image);
    }
}

void ImageCache::addImageWithView(
    const std::string& key, std::unique_ptr<VulkanImage> image, std::unique_ptr<VulkanImageView> imageView)
{
    auto& cachedView = m_imageViews[key];
    if (!cachedView)
    {
        cachedView = std::move(imageView);
    }

    auto& cachedImage = m_images[key];
    if (!cachedImage)
    {
        cachedImage = std::move(image);
    }
}

void ImageCache::addImage(const std::string& key, std::unique_ptr<VulkanImage> image)
{
    auto& cachedImage = m_images[key];
    if (!cachedImage)
    {
        cachedImage = std::move(image);
    }
}

VulkanImage& ImageCache::getImage(const std::string& key) const
{
    return *m_images.at(key);
}

void ImageCache::removeImage(const std::string& key)
{
    m_images.erase(key);
}

void ImageCache::removeImageView(const std::string& key)
{
    m_imageViews.erase(key);
}

void ImageCache::addImageView(const std::string& key, std::unique_ptr<VulkanImageView> imageView)
{
    auto& cachedView = m_imageViews[key];
    if (!cachedView)
    {
        cachedView = std::move(imageView);
    }
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