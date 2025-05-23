#pragma once

#include <filesystem>

#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Renderer/ImageCache.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>

namespace crisp {
std::pair<PbrMaterial, PbrImageGroup> loadPbrMaterial(const std::filesystem::path& materialDir);

void addPbrImageGroupToImageCache(const PbrImageGroup& imageGroup, ImageCache& imageCache);

std::unique_ptr<VulkanImage> createSheenLookup(Renderer& renderer, const std::filesystem::path& assetDir);

Material* createPbrMaterial(
    std::string_view materialId,
    const PbrMaterial& pbrMaterial,
    ResourceContext& resourceContext,
    const TransformBuffer& transformBuffer);

void configureForwardLightingPassMaterial(
    Material& material,
    const ResourceContext& resourceContext,
    const LightSystem& lightSystem,
    const rg::RenderGraph& rg);

} // namespace crisp
