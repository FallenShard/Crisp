#include <Crisp/Materials/PbrMaterialUtils.hpp>

#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Common/Logger.hpp>
#include <Crisp/IO/ImageLoader.hpp>

namespace crisp
{
namespace
{
const auto logger = createLoggerMt("PbrMaterial");

struct TexInfo
{
    std::string_view name;
    VkFormat defaultFormat;
};

constexpr TexInfo AlbedoTex = {"diffuse", VK_FORMAT_R8G8B8A8_SRGB};
constexpr TexInfo NormalTex = {"normal", VK_FORMAT_R8G8B8A8_UNORM};
constexpr TexInfo MetallicTex = {"metallic", VK_FORMAT_R8_UNORM};
constexpr TexInfo RoughnessTex = {"roughness", VK_FORMAT_R8_UNORM};
constexpr TexInfo OcclusionTex = {"ao", VK_FORMAT_R8_UNORM};
constexpr TexInfo EmissionTex = {"emissive", VK_FORMAT_R8G8B8A8_UNORM};

} // namespace

PbrTextureGroup loadPbrTextureGroup(const std::filesystem::path& materialDir)
{
    const auto loadTextureOpt = [&materialDir](const std::string_view texName) -> std::optional<Image>
    {
        const auto& path = materialDir / fmt::format("{}.png", texName);
        if (std::filesystem::exists(path))
        {
            return loadImage(path, 4, FlipOnLoad::Y).unwrap();
        }
        else
        {
            logger->warn("Failed to load texture from {}.", path.string());
        }

        return std::nullopt;
    };

    PbrTextureGroup textureGroup{};
    textureGroup.albedo = loadTextureOpt(AlbedoTex.name);
    textureGroup.normal = loadTextureOpt(NormalTex.name);
    textureGroup.roughness = loadTextureOpt(RoughnessTex.name);
    textureGroup.metallic = loadTextureOpt(MetallicTex.name);
    textureGroup.occlusion = loadTextureOpt(OcclusionTex.name);
    textureGroup.emissive = loadTextureOpt(EmissionTex.name);
    return textureGroup;
}

void addToImageCache(const PbrTextureGroup& texGroup, const std::string& materialKey, ImageCache& imageCache)
{
    const auto addTex =
        [&imageCache, &texGroup, &materialKey](const std::optional<Image>& texture, const TexInfo& texInfo)
    {
        if (texture)
        {
            const std::string key = fmt::format("{}-{}", materialKey, texInfo.name);
            imageCache.addImageWithView(key, createTexture(imageCache.getRenderer(), *texture, texInfo.defaultFormat));
        }
    };

    addTex(texGroup.albedo, AlbedoTex);
    addTex(texGroup.normal, NormalTex);
    addTex(texGroup.roughness, RoughnessTex);
    addTex(texGroup.metallic, MetallicTex);
    addTex(texGroup.occlusion, OcclusionTex);
    addTex(texGroup.emissive, EmissionTex);
}

void addDefaultPbrTexturesToImageCache(ImageCache& imageCache)
{
    PbrTextureGroup textureGroup{};
    textureGroup.albedo =
        loadImage(imageCache.getRenderer()->getResourcesPath() / "Textures/uv_pattern.jpg", 4, FlipOnLoad::Y).unwrap();
    textureGroup.normal = createDefaultNormalMap();
    textureGroup.roughness = createDefaultRoughnessMap();
    textureGroup.metallic = createDefaultMetallicMap();
    textureGroup.occlusion = createDefaultAmbientOcclusionMap();
    textureGroup.emissive = createDefaultEmissiveMap();
    addToImageCache(textureGroup, "default", imageCache);
}

} // namespace crisp
