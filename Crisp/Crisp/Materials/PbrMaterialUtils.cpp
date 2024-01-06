#include <Crisp/Materials/PbrMaterialUtils.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Image/Io/Exr.hpp>
#include <Crisp/Image/Io/Utils.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

namespace crisp {
namespace {
const auto logger = createLoggerMt("PbrMaterial");

struct TexInfo {
    std::string_view name;
    VkFormat defaultFormat;
};

constexpr TexInfo kAlbedoTex = {"diffuse", VK_FORMAT_R8G8B8A8_SRGB};
constexpr TexInfo kNormalTex = {"normal", VK_FORMAT_R8G8B8A8_UNORM};
constexpr TexInfo kMetallicTex = {"metallic", VK_FORMAT_R8_UNORM};
constexpr TexInfo kRoughnessTex = {"roughness", VK_FORMAT_R8_UNORM};
constexpr TexInfo kOcclusionTex = {"ao", VK_FORMAT_R8_UNORM};
constexpr TexInfo kEmissionTex = {"emissive", VK_FORMAT_R8G8B8A8_UNORM};

} // namespace

PbrTextureGroup loadPbrTextureGroup(const std::filesystem::path& materialDir) {
    const auto loadTextureOpt =
        [&materialDir](const std::string_view name, const uint32_t requestedChannels) -> std::optional<Image> {
        const auto& path = materialDir / fmt::format("{}.png", name);
        if (std::filesystem::exists(path)) {
            return loadImage(path, static_cast<int32_t>(requestedChannels), FlipAxis::Y).unwrap();
        }

        logger->warn("Failed to load texture from {}.", path.string());
        return std::nullopt;
    };

    PbrTextureGroup textureGroup{};
    textureGroup.albedo = loadTextureOpt(kAlbedoTex.name, getNumChannels(kAlbedoTex.defaultFormat));
    textureGroup.normal = loadTextureOpt(kNormalTex.name, getNumChannels(kNormalTex.defaultFormat));
    textureGroup.roughness = loadTextureOpt(kRoughnessTex.name, getNumChannels(kRoughnessTex.defaultFormat));
    textureGroup.metallic = loadTextureOpt(kMetallicTex.name, getNumChannels(kMetallicTex.defaultFormat));
    textureGroup.occlusion = loadTextureOpt(kOcclusionTex.name, getNumChannels(kOcclusionTex.defaultFormat));
    textureGroup.emissive = loadTextureOpt(kEmissionTex.name, getNumChannels(kEmissionTex.defaultFormat));
    return textureGroup;
}

void addPbrTexturesToImageCache(
    const PbrTextureGroup& texGroup, const std::string& materialKey, ImageCache& imageCache) {
    const auto addTex = [&imageCache, &materialKey](const std::optional<Image>& texture, const TexInfo& texInfo) {
        if (texture) {
            const std::string key = fmt::format("{}-{}", materialKey, texInfo.name);
            imageCache.addImageWithView(
                key, createVulkanImage(*imageCache.getRenderer(), *texture, texInfo.defaultFormat));
        }
    };

    addTex(texGroup.albedo, kAlbedoTex);
    addTex(texGroup.normal, kNormalTex);
    addTex(texGroup.roughness, kRoughnessTex);
    addTex(texGroup.metallic, kMetallicTex);
    addTex(texGroup.occlusion, kOcclusionTex);
    addTex(texGroup.emissive, kEmissionTex);
}

void removePbrTexturesFromImageCache(const std::string& materialKey, ImageCache& imageCache) {
    logger->info("Removing PBR textures with key: {}", materialKey);
    const auto removeTexAndView = [&imageCache, &materialKey](const TexInfo& texInfo) {
        const std::string key = fmt::format("{}-{}", materialKey, texInfo.name);
        imageCache.removeImage(key);
        imageCache.removeImageView(key);
    };

    removeTexAndView(kAlbedoTex);
    removeTexAndView(kNormalTex);
    removeTexAndView(kRoughnessTex);
    removeTexAndView(kMetallicTex);
    removeTexAndView(kOcclusionTex);
    removeTexAndView(kEmissionTex);
}

std::unique_ptr<VulkanImage> createSheenLookup(Renderer& renderer, const std::filesystem::path& assetDir) {
    const auto exr = loadExr(assetDir / "Textures/Sheen_E.exr").unwrap();
    VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = VK_FORMAT_R32_SFLOAT;
    createInfo.extent = {exr.width, exr.height, 1};
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return createVulkanImage(renderer, exr.pixelData.size() * sizeof(float), exr.pixelData.data(), createInfo);
}

Material* createPbrMaterial(
    const std::string_view materialId,
    const std::string_view pbrTexturePrefix,
    ResourceContext& resourceContext,
    const PbrParams& params,
    const TransformBuffer& transformBuffer) {
    auto& imageCache = resourceContext.imageCache;

    auto* material = resourceContext.createMaterial(fmt::format("pbrTex{}", materialId), "pbrTex");
    material->writeDescriptor(0, 0, transformBuffer.getDescriptorInfo());

    const auto setMaterialTexture =
        [&material, &imageCache, &pbrTexturePrefix](const uint32_t index, const std::string_view texName) {
            const std::string key = fmt::format("{}-{}", pbrTexturePrefix, texName);
            const std::string fallbackKey = fmt::format("default-{}", texName);
            material->writeDescriptor(
                2, index, imageCache.getImageView(key, fallbackKey), imageCache.getSampler("linearRepeat"));
        };

    setMaterialTexture(0, "diffuse");
    setMaterialTexture(1, "metallic");
    setMaterialTexture(2, "roughness");
    setMaterialTexture(3, "normal");
    setMaterialTexture(4, "ao");
    setMaterialTexture(5, "emissive");

    const std::string paramsBufferKey{fmt::format("{}-params", materialId)};
    material->writeDescriptor(
        2, 6, *resourceContext.createUniformBuffer(paramsBufferKey, params, BufferUpdatePolicy::PerFrame));

    return material;
}

Material* createPbrMaterial(
    const std::string_view materialId,
    const std::array<int32_t, 6>& textureIndices,
    ResourceContext& resourceContext,
    const PbrParams& params,
    const TransformBuffer& transformBuffer) {
    auto& imageCache = resourceContext.imageCache;

    auto* material = resourceContext.createMaterial(fmt::format("pbrTex{}", materialId), "pbrTex");
    material->writeDescriptor(0, 0, transformBuffer.getDescriptorInfo());

    const auto setMaterialTexture =
        [&material, &imageCache](const uint32_t index, const std::string_view texName, const std::string& texKey) {
            const std::string fallbackKey = fmt::format("default-{}", texName);
            material->writeDescriptor(
                2, index, imageCache.getImageView(texKey, fallbackKey), imageCache.getSampler("linearRepeat"));
        };

    setMaterialTexture(0, "diffuse", fmt::format("IMAGE-ALBEDO-{}", textureIndices[0]));
    setMaterialTexture(1, "metallic", fmt::format("IMAGE-METALLIC-{}", textureIndices[2]));
    setMaterialTexture(2, "roughness", fmt::format("IMAGE-ROUGHNESS-{}", textureIndices[3]));
    setMaterialTexture(3, "normal", fmt::format("IMAGE-NORMAL-{}", textureIndices[1]));
    setMaterialTexture(4, "ao", fmt::format("IMAGE-AO-{}", textureIndices[4]));
    setMaterialTexture(5, "emissive", fmt::format("IMAGE-EMISSIVE-{}", textureIndices[5]));

    const std::string paramsBufferKey{fmt::format("{}-params", materialId)};
    material->writeDescriptor(
        2, 6, *resourceContext.createUniformBuffer(paramsBufferKey, params, BufferUpdatePolicy::PerFrame));

    return material;
};

void setPbrMaterialSceneParams(
    Material& material,
    const ResourceContext& resourceContext,
    const LightSystem& lightSystem,
    const rg::RenderGraph& rg) {
    const auto& imageCache = resourceContext.imageCache;
    const auto& envLight = *lightSystem.getEnvironmentLight();
    material.writeDescriptor(1, 0, *resourceContext.getUniformBuffer("camera"));
    material.writeDescriptor(1, 1, *lightSystem.getCascadedDirectionalLightBuffer());
    material.writeDescriptor(1, 2, envLight.getDiffuseMapView(), imageCache.getSampler("linearClamp"));
    material.writeDescriptor(1, 3, envLight.getSpecularMapView(), imageCache.getSampler("linearMipmap"));
    material.writeDescriptor(1, 4, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));
    material.writeDescriptor(1, 5, imageCache.getImageView("sheenLut"), imageCache.getSampler("linearClamp"));
    for (uint32_t i = 0; i < kDefaultCascadeCount; ++i) {
        for (uint32_t k = 0; k < kRendererVirtualFrameCount; ++k) {
            const auto& shadowMapView{rg.getRenderPass(kCsmPasses[i]).getAttachmentView(0, k)};
            material.writeDescriptor(1, 6, k, i, shadowMapView, &imageCache.getSampler("nearestNeighbor"));
            material.getRenderPassBindings().emplace_back(1, 6, k, i, kCsmPasses[i], 0, "nearestNeighbor");
        }
    }
}

} // namespace crisp
