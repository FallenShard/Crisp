#include <Crisp/Materials/PbrMaterial.hpp>

#include <algorithm>

#include <Crisp/Core/Checks.hpp>

namespace crisp {
Image createDefaultAlbedoMap(const std::array<uint8_t, 4>& color) {
    return {std::vector<uint8_t>(color.begin(), color.end()), 1, 1, 4, 4 * sizeof(uint8_t)};
}

Image createDefaultNormalMap() {
    return {std::vector<uint8_t>{128, 128, 255, 255}, 1, 1, 4, 4 * sizeof(uint8_t)};
}

Image createDefaultOrmMap() {
    return {std::vector<uint8_t>{255, 255, 0, 255}, 1, 1, 4, 4 * sizeof(uint8_t)};
}

Image createDefaultEmissiveMap() {
    return {std::vector<uint8_t>{255, 255, 255, 255}, 1, 1, 4, 4 * sizeof(uint8_t)};
}

Image createPbrOrmMap(const PbrOrmSources& sources) {
    const Image* extentSource =
        sources.roughness != nullptr ? sources.roughness
        : sources.metallic != nullptr
            ? sources.metallic
            : sources.occlusion;
    if (extentSource == nullptr) {
        return createDefaultOrmMap();
    }

    const uint32_t width = extentSource->getWidth();
    const uint32_t height = extentSource->getHeight();
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);

    const auto sampleChannel =
        [width, height](
            const Image* image, const uint32_t channel, const uint32_t x, const uint32_t y, const uint8_t fallback) {
            if (image == nullptr) {
                return fallback;
            }

            CRISP_CHECK_LT(channel, image->getChannelCount());
            CRISP_CHECK_EQ(image->getPixelByteSize(), image->getChannelCount());
            const uint32_t sourceX = std::min(x * image->getWidth() / width, image->getWidth() - 1);
            const uint32_t sourceY = std::min(y * image->getHeight() / height, image->getHeight() - 1);
            const size_t offset =
                (static_cast<size_t>(sourceY) * image->getWidth() + sourceX) * image->getPixelByteSize() + channel;
            return image->getData()[offset];
        };

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
            pixels[offset + 0] = sampleChannel(sources.occlusion, sources.occlusionChannel, x, y, 255);
            pixels[offset + 1] = sampleChannel(sources.roughness, sources.roughnessChannel, x, y, 255);
            pixels[offset + 2] = sampleChannel(sources.metallic, sources.metallicChannel, x, y, 0);
            pixels[offset + 3] = 255;
        }
    }

    return {std::move(pixels), width, height, 4, 4 * sizeof(uint8_t)};
}

PbrImageGroup createDefaultPbrImageGroup() {
    return {
        .name = "default",
        .albedoMaps = {createDefaultAlbedoMap(std::array<uint8_t, 4>{255, 255, 255, 255})},
        .normalMaps = {createDefaultNormalMap()},
        .ormMaps = {createDefaultOrmMap()},
        .emissiveMaps = {createDefaultEmissiveMap()},
    };
}
} // namespace crisp
