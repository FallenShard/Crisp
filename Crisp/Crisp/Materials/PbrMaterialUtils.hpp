#pragma once

#include <filesystem>

#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Materials/PbrMaterialTable.hpp>
#include <Crisp/Renderer/ImageCache.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>

namespace crisp {
std::pair<PbrMaterial, PbrImageGroup> loadPbrMaterial(const std::filesystem::path& materialDir);

void addPbrImageGroupToImageCache(const PbrImageGroup& imageGroup, ImageCache& imageCache);

PbrParams createGpuPbrParams(const PbrMaterial& pbrMaterial, const ImageCache& imageCache);

void configureForwardLightingPassMaterial(
    Material& material,
    const ResourceContext& resourceContext,
    const LightSystem& lightSystem,
    const rg::RenderGraph& rg);

} // namespace crisp
