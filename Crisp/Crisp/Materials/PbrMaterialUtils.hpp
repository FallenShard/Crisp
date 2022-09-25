#pragma once

#include <Crisp/Renderer/ImageCache.hpp>

#include <Crisp/Materials/PbrMaterial.hpp>

#include <filesystem>

namespace crisp
{
class Renderer;

PbrTextureGroup loadPbrTextureGroup(const std::filesystem::path& materialDir);

void addToImageCache(const PbrTextureGroup& texGroup, const std::string& materialKey, ImageCache& imageCache);
void addDefaultPbrTexturesToImageCache(ImageCache& imageCache);

std::unique_ptr<VulkanImage> createSheenLookup(Renderer& renderer, const std::filesystem::path& assetDir);

} // namespace crisp
