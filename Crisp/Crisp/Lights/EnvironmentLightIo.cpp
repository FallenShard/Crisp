#include <Crisp/Lights/EnvironmentLightIo.hpp>

#include <Crisp/Image/Io/Utils.hpp>

namespace crisp {

Result<ImageBasedLightingData> loadImageBasedLightingData(const std::filesystem::path& envMapDir) {
    const auto envMapName{envMapDir.stem().string()};
    ImageBasedLightingData data{};

    constexpr uint32_t kRequestedChannels{4};
    data.equirectangularEnvironmentMap =
        loadImage(envMapDir / fmt::format("{}.hdr", envMapName), kRequestedChannels, FlipOnLoad::Y).unwrap();

    const std::filesystem::path diffMapPath{envMapDir / fmt::format("{}_irr.hdr", envMapName)};
    data.diffuseIrradianceCubeMap = loadCubeMapFacesFromHCrossImage(diffMapPath);

    constexpr uint32_t kReflectionMipMapLevels{9};
    for (uint32_t i = 0; i < kReflectionMipMapLevels; ++i) {
        const uint32_t width = (1 << (kReflectionMipMapLevels - i)) * 4;
        const uint32_t height = (1 << (kReflectionMipMapLevels - i)) * 3;
        const std::filesystem::path reflMapPath{
            envMapDir / fmt::format("{}_rad_{}_{}x{}.hdr", envMapName, i, width, height)};
        data.specularReflectanceMapMipLevels.emplace_back(loadCubeMapFacesFromHCrossImage(reflMapPath));
    }
    return data;
}

} // namespace crisp
