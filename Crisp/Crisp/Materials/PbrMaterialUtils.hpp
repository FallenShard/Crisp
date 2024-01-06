#pragma once

#include <filesystem>

#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Renderer/ImageCache.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>

namespace crisp {
PbrTextureGroup loadPbrTextureGroup(const std::filesystem::path& materialDir);

void addPbrTexturesToImageCache(const PbrTextureGroup& texGroup, const std::string& materialKey, ImageCache& imageCache);
void removePbrTexturesFromImageCache(const std::string& materialKey, ImageCache& imageCache);

std::unique_ptr<VulkanImage> createSheenLookup(Renderer& renderer, const std::filesystem::path& assetDir);

Material* createPbrMaterial(
    std::string_view materialId,
    std::string_view pbrTexturePrefix,
    ResourceContext& resourceContext,
    const PbrParams& params,
    const TransformBuffer& transformBuffer);

Material* createPbrMaterial(
    std::string_view materialId,
    const std::array<int32_t, 6>& textureIndices,
    ResourceContext& resourceContext,
    const PbrParams& params,
    const TransformBuffer& transformBuffer);

void setPbrMaterialSceneParams(
    Material& material,
    const ResourceContext& resourceContext,
    const LightSystem& lightSystem,
    const rg::RenderGraph& rg);

} // namespace crisp
