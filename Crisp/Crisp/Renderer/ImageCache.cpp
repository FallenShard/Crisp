#include <Crisp/Renderer/ImageCache.hpp>

#include <Crisp/Renderer/Renderer.hpp>

namespace crisp {

ImageCache::ImageCache(Renderer* renderer)
    : m_renderer{renderer} {}

void ImageCache::addImage(const std::string& key, std::unique_ptr<VulkanImage> image) {
    auto& cachedImage = m_images[key];
    if (!cachedImage) {
        cachedImage = std::move(image);
    }
}

VulkanImage& ImageCache::getImage(const std::string& key) const {
    return *m_images.at(key);
}

void ImageCache::removeImage(const std::string& key) {
    m_images.erase(key);
}

void ImageCache::removeImageView(const std::string& key) {
    m_imageViews.erase(key);
}

void ImageCache::addImageView(const std::string& key, std::unique_ptr<VulkanImageView> imageView) {
    auto& cachedView = m_imageViews[key];
    if (!cachedView) {
        cachedView = std::move(imageView);
    }
}

VulkanImageView& ImageCache::getImageView(const std::string& key) const {
    if (auto found = m_images.find(key); found != m_images.end()) {
        return found->second->getView();
    }
    return *m_imageViews.at(key);
}

VulkanImageView& ImageCache::getImageView(const std::string& key, const std::string& fallbackKey) const {
    if (auto found = m_images.find(key); found != m_images.end()) {
        return found->second->getView();
    }
    if (auto found = m_images.find(fallbackKey); found != m_images.end()) {
        return found->second->getView();
    }
    if (auto found = m_imageViews.find(key); found != m_imageViews.end()) {
        return *found->second;
    }

    return *m_imageViews.at(fallbackKey);
}

void ImageCache::addSampler(const std::string& key, std::unique_ptr<VulkanSampler> sampler) {
    auto& cachedSampler = m_samplers[key];
    cachedSampler = std::move(sampler);
    m_samplerIndices[key] = m_renderer->getBindlessImageRegistry().addSampler(*cachedSampler, key);
}

VulkanSampler& ImageCache::getSampler(const std::string& key) const {
    return *m_samplers.at(key);
}

uint32_t ImageCache::getSamplerIndex(const std::string& key) const {
    return m_samplerIndices.at(key);
}

SampledImageHandle ImageCache::registerBindlessImage(const std::string& key) {
    if (const auto found = m_imageHandles.find(key); found != m_imageHandles.end()) {
        return found->second;
    }

    const auto handle = m_renderer->getBindlessImageRegistry().addSampledImage(getImageView(key), key);
    m_imageHandles.emplace(key, handle);
    return handle;
}

SampledImageHandle ImageCache::getImageHandle(const std::string& key, const std::string& fallbackKey) const {
    if (const auto found = m_imageHandles.find(key); found != m_imageHandles.end()) {
        return found->second;
    }
    if (const auto found = m_imageHandles.find(fallbackKey); found != m_imageHandles.end()) {
        return found->second;
    }

    return {BindlessImageRegistry::kDefaultSlot, 0};
}

} // namespace crisp