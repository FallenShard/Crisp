#include <Crisp/Materials/PbrMaterialUtils.hpp>

#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Common/Logger.hpp>
#include <Crisp/IO/ImageLoader.hpp>
#include <Crisp/IO/OpenEXRReader.hpp>

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
            imageCache.addImageWithView(
                key, convertToVulkanImage(imageCache.getRenderer(), *texture, texInfo.defaultFormat));
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

std::unique_ptr<VulkanImage> createSheenLookup(Renderer& renderer, const std::filesystem::path& assetDir)
{
    OpenEXRReader reader;
    std::vector<float> buffer;
    uint32_t w, h;
    reader.read(assetDir / "Textures/Sheen_E.exr", buffer, w, h);
    VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = VK_FORMAT_R32_SFLOAT;
    createInfo.extent = {w, h, 1u};
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    return createVulkanImage(renderer, buffer.size() * sizeof(float), buffer.data(), createInfo);
}

} // namespace crisp
