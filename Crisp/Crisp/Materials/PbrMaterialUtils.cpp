#include <Crisp/Materials/PbrMaterialUtils.hpp>

#include <Crisp/Renderer/VulkanImageUtils.hpp>
#include <Crisp/Vulkan/VulkanFormatTraits.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Image/Io/OpenEXRReader.hpp>
#include <Crisp/Image/Io/Utils.hpp>

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
    const auto loadTextureOpt =
        [&materialDir](const std::string_view name, const uint32_t requestedChannels) -> std::optional<Image>
    {
        const auto& path = materialDir / fmt::format("{}.png", name);
        if (std::filesystem::exists(path))
        {
            return loadImage(path, requestedChannels, FlipOnLoad::Y).unwrap();
        }
        else
        {
            logger->warn("Failed to load texture from {}.", path.string());
        }

        return std::nullopt;
    };

    PbrTextureGroup textureGroup{};
    textureGroup.albedo = loadTextureOpt(AlbedoTex.name, getNumChannels(AlbedoTex.defaultFormat));
    textureGroup.normal = loadTextureOpt(NormalTex.name, getNumChannels(NormalTex.defaultFormat));
    textureGroup.roughness = loadTextureOpt(RoughnessTex.name, getNumChannels(RoughnessTex.defaultFormat));
    textureGroup.metallic = loadTextureOpt(MetallicTex.name, getNumChannels(MetallicTex.defaultFormat));
    textureGroup.occlusion = loadTextureOpt(OcclusionTex.name, getNumChannels(OcclusionTex.defaultFormat));
    textureGroup.emissive = loadTextureOpt(EmissionTex.name, getNumChannels(EmissionTex.defaultFormat));
    return textureGroup;
}

void addPbrTexturesToImageCache(const PbrTextureGroup& texGroup, const std::string& materialKey, ImageCache& imageCache)
{
    const auto addTex =
        [&imageCache, &texGroup, &materialKey](const std::optional<Image>& texture, const TexInfo& texInfo)
    {
        if (texture)
        {
            const std::string key = fmt::format("{}-{}", materialKey, texInfo.name);
            imageCache.addImageWithView(
                key, createVulkanImage(*imageCache.getRenderer(), *texture, texInfo.defaultFormat));
        }
    };

    addTex(texGroup.albedo, AlbedoTex);
    addTex(texGroup.normal, NormalTex);
    addTex(texGroup.roughness, RoughnessTex);
    addTex(texGroup.metallic, MetallicTex);
    addTex(texGroup.occlusion, OcclusionTex);
    addTex(texGroup.emissive, EmissionTex);
}

void removePbrTexturesFromImageCache(const std::string& materialKey, ImageCache& imageCache)
{
    logger->info("Removing PBR textures with key: {}", materialKey);
    const auto removeTexAndView = [&imageCache, &materialKey](const TexInfo& texInfo)
    {
        const std::string key = fmt::format("{}-{}", materialKey, texInfo.name);
        imageCache.removeImage(key);
        imageCache.removeImageView(key);
    };

    removeTexAndView(AlbedoTex);
    removeTexAndView(NormalTex);
    removeTexAndView(RoughnessTex);
    removeTexAndView(MetallicTex);
    removeTexAndView(OcclusionTex);
    removeTexAndView(EmissionTex);
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
