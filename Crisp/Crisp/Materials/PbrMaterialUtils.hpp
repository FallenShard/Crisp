#pragma once

#include <Crisp/Renderer/ImageCache.hpp>

#include <Crisp/Materials/PbrMaterial.hpp>

#include <filesystem>

namespace crisp
{

PbrTextureGroup loadPbrTextureGroup(const std::filesystem::path& materialDir);

void addToImageCache(const PbrTextureGroup& texGroup, const std::string& materialKey, ImageCache& imageCache);
void addDefaultPbrTexturesToImageCache(ImageCache& imageCache);

} // namespace crisp
