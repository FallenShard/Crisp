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

consteval auto createTexInfos() {
    std::array<TexInfo, kPbrMapTypeCount> texInfos{};
    for (uint32_t i = 0; i < kPbrMapTypeCount; ++i) {
        texInfos[i].name = kPbrMapNames[i];
    }
    texInfos[kPbrAlbedoMapIndex].defaultFormat = VK_FORMAT_R8G8B8A8_SRGB;
    texInfos[kPbrNormalMapIndex].defaultFormat = VK_FORMAT_R8G8B8A8_UNORM;
    texInfos[kPbrMetallicMapIndex].defaultFormat = VK_FORMAT_R8_UNORM;
    texInfos[kPbrRoughnessMapIndex].defaultFormat = VK_FORMAT_R8_UNORM;
    texInfos[kPbrOcclusionMapIndex].defaultFormat = VK_FORMAT_R8_UNORM;
    texInfos[kPbrEmissiveMapIndex].defaultFormat = VK_FORMAT_R8G8B8A8_SRGB;
    return texInfos;
}

constexpr std::array<TexInfo, kPbrMapTypeCount> kTexInfos = createTexInfos();

const std::vector<FlatStringHashSet> kTextureFileAliases = {
    {"diffuse"},
    {},
    {},
    {},
    {"ao"},
    {},
};

} // namespace

PbrImageGroup loadPbrImageGroup(const std::filesystem::path& materialDir, std::string name) {
    const auto loadImageIfExists =
        [&materialDir](
            std::vector<Image>& images,
            const std::string_view filename,
            const FlatStringHashSet& aliases,
            const uint32_t requestedChannels) {
            const auto& path = materialDir / fmt::format("{}.png", filename);
            if (std::filesystem::exists(path)) {
                images.push_back(loadImage(path, static_cast<int32_t>(requestedChannels), FlipAxis::Y).unwrap());
                return;
            }
            for (const auto& alias : aliases) {
                const auto& aliasPath = materialDir / fmt::format("{}.png", alias);
                images.push_back(loadImage(aliasPath, static_cast<int32_t>(requestedChannels), FlipAxis::Y).unwrap());
                return;
            }

            logger->warn("Image does not exist at path: '{}'.", path.string());
        };

    PbrImageGroup group{};
    group.name = std::move(name);
    std::array<std::vector<Image>*, kPbrMapTypeCount> mapArrays{
        &group.albedoMaps,
        &group.normalMaps,
        &group.roughnessMaps,
        &group.metallicMaps,
        &group.occlusionMaps,
        &group.emissiveMaps,
    };
    for (auto&& [idx, mapArray] : std::views::enumerate(mapArrays)) {
        loadImageIfExists(
            *mapArray, kTexInfos[idx].name, kTextureFileAliases[idx], getNumChannels(kTexInfos[idx].defaultFormat));
    }
    return group;
}

std::pair<PbrMaterial, PbrImageGroup> loadPbrMaterial(const std::filesystem::path& materialDir) {

    PbrMaterial material{.name = materialDir.stem().string()};

    auto group = loadPbrImageGroup(materialDir, material.name);
    const PbrImageKeyCreator keyCreator{group.name};
    material.textureKeys[0] = group.albedoMaps.empty() ? "" : keyCreator.createAlbedoMapKey(0);
    material.textureKeys[1] = group.normalMaps.empty() ? "" : keyCreator.createNormalMapKey(0);
    material.textureKeys[2] = group.roughnessMaps.empty() ? "" : keyCreator.createRoughnessMapKey(0);
    material.textureKeys[3] = group.metallicMaps.empty() ? "" : keyCreator.createMetallicMapKey(0);
    material.textureKeys[4] = group.occlusionMaps.empty() ? "" : keyCreator.createOcclusionMapKey(0);
    material.textureKeys[5] = group.emissiveMaps.empty() ? "" : keyCreator.createEmissiveMapKey(0);

    return {std::move(material), std::move(group)};
}

void addPbrImageGroupToImageCache(const PbrImageGroup& imageGroup, ImageCache& imageCache) {
    auto& renderer = *imageCache.getRenderer();
    const PbrImageKeyCreator keyCreator{imageGroup.name};
    std::array<const std::vector<Image>*, kPbrMapTypeCount> mapArrays{
        &imageGroup.albedoMaps,
        &imageGroup.normalMaps,
        &imageGroup.roughnessMaps,
        &imageGroup.metallicMaps,
        &imageGroup.occlusionMaps,
        &imageGroup.emissiveMaps,
    };

    for (auto&& [typeIdx, maps] : std::views::enumerate(mapArrays)) {
        const uint32_t mapTypeIdx{static_cast<uint32_t>(typeIdx)};
        for (auto&& [idx, data] : std::views::enumerate(*maps)) {
            imageCache.addImageWithView(
                keyCreator.createMapKey(mapTypeIdx, static_cast<uint32_t>(idx)),
                createVulkanImage(renderer, data, kTexInfos[typeIdx].defaultFormat));
        }
    };
}

void removePbrTexturesFromImageCache(const std::string& imageGroupName, ImageCache& imageCache) {
    logger->info("Removing PBR images with key: {}", imageGroupName);
    const auto removeTexAndView = [&imageCache, &imageGroupName](const std::string_view mapName) {
        const std::string key = fmt::format("{}-{}-0", imageGroupName, mapName);
        imageCache.removeImage(key);
        imageCache.removeImageView(key);
    };

    for (const auto& mapName : kPbrMapNames) {
        removeTexAndView(mapName);
    }
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
    const PbrMaterial& pbrMaterial,
    ResourceContext& resourceContext,
    const TransformBuffer& transformBuffer) {
    auto& imageCache = resourceContext.imageCache;

    auto* material = resourceContext.createMaterial(fmt::format("pbr-{}", materialId), "pbr");
    material->writeDescriptor(2, 0, transformBuffer.getDescriptorInfo());

    const auto setMaterialTexture =
        [&material, &imageCache](const uint32_t index, const std::string_view texName, const std::string& texKey) {
            const std::string fallbackKey = fmt::format("default-{}-0", texName);
            // material->writeDescriptor(
            //     1, index, imageCache.getImageView(texKey, fallbackKey), imageCache.getSampler("linearRepeat"));
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.descriptorCount = 1;
            write.dstArrayElement = index;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.dstBinding = 1;

            material->writeDescriptor(
                1,
                write,
                imageCache.getImageView(texKey, fallbackKey).getDescriptorInfo(&imageCache.getSampler("linearRepeat")));
        };

    for (uint32_t i = 0; i < kPbrMapTypeCount; ++i) {
        setMaterialTexture(i, kPbrMapNames[i], pbrMaterial.textureKeys[i]);
    }

    const std::string paramsBufferKey{fmt::format("{}-params", materialId)};
    material->writeDescriptor(
        1, 0, *resourceContext.createUniformBuffer(paramsBufferKey, pbrMaterial.params, BufferUpdatePolicy::PerFrame));

    return material;
}

void setPbrMaterialSceneParams(
    Material& material,
    const ResourceContext& resourceContext,
    const LightSystem& lightSystem,
    const rg::RenderGraph& rg) {
    const auto& imageCache = resourceContext.imageCache;
    const auto& envLight = *lightSystem.getEnvironmentLight();
    material.writeDescriptor(0, 0, *resourceContext.getUniformBuffer("camera"));
    material.writeDescriptor(0, 1, *lightSystem.getCascadedDirectionalLightBuffer());
    material.writeDescriptor(0, 2, envLight.getDiffuseMapView(), imageCache.getSampler("linearClamp"));
    material.writeDescriptor(0, 3, envLight.getSpecularMapView(), imageCache.getSampler("linearMipmap"));
    material.writeDescriptor(0, 5, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));
    material.writeDescriptor(0, 6, imageCache.getImageView("sheenLut"), imageCache.getSampler("linearClamp"));
    for (uint32_t i = 0; i < kDefaultCascadeCount; ++i) {
        for (uint32_t k = 0; k < kRendererVirtualFrameCount; ++k) {
            const auto& shadowMapView{rg.getRenderPass(kCsmPasses[i]).getAttachmentView(0, k)};
            material.writeDescriptor(0, 4, k, i, shadowMapView, &imageCache.getSampler("nearestNeighbor"));
            material.getRenderPassBindings().emplace_back(0, 4, k, i, kCsmPasses[i], 0, "nearestNeighbor");
        }
    }
}

} // namespace crisp
