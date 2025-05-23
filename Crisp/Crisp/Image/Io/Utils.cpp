#include <Crisp/Image/Io/Utils.hpp>

#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4244) // conversion warnings
#include <stb/stb_image.h>
#pragma warning(pop)

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Format.hpp>

namespace crisp {
namespace {
auto getStbComponentFormat(const int numComponents) {
    switch (numComponents) {
    case 0:
        return STBI_default;
    case 1:
        return STBI_grey;
    case 2:
        return STBI_grey_alpha;
    case 3:
        return STBI_rgb;
    case 4:
        return STBI_rgb_alpha;
    default:
        return STBI_default;
    }
}

constexpr std::size_t kNumCubeMapFaces = 6;
const std::array<const std::string, kNumCubeMapFaces> kSideFilenames = {
    "left", "right", "top", "bottom", "back", "front"};
} // namespace

std::vector<Image> loadCubeMapFacesFromHCrossImage(const std::filesystem::path& path, const FlipAxis flip) {
    const Image hcrossImage{loadImage(path, 4, flip).unwrap()};
    const uint32_t cubeMapSize{hcrossImage.getWidth() / 4};
    CRISP_CHECK(
        cubeMapSize == hcrossImage.getHeight() / 3,
        "{} {} {}",
        cubeMapSize,
        hcrossImage.getWidth(),
        hcrossImage.getHeight());

    std::vector<Image> cubeMapFaces;
    // Right, Left, Top, Bottom, Front, Back.
    cubeMapFaces.push_back(hcrossImage.createSubImage(cubeMapSize, 2 * cubeMapSize, cubeMapSize, cubeMapSize));
    cubeMapFaces.push_back(hcrossImage.createSubImage(cubeMapSize, 0, cubeMapSize, cubeMapSize));
    cubeMapFaces.push_back(hcrossImage.createSubImage(0, cubeMapSize, cubeMapSize, cubeMapSize));
    cubeMapFaces.push_back(hcrossImage.createSubImage(2 * cubeMapSize, cubeMapSize, cubeMapSize, cubeMapSize));
    cubeMapFaces.push_back(hcrossImage.createSubImage(cubeMapSize, cubeMapSize, cubeMapSize, cubeMapSize));
    cubeMapFaces.push_back(hcrossImage.createSubImage(cubeMapSize, 3 * cubeMapSize, cubeMapSize, cubeMapSize));
    return cubeMapFaces;
}

Result<std::vector<Image>> loadCubeMapFaces(const std::filesystem::path& path) {
    std::vector<Image> cubeMapImages;
    cubeMapImages.reserve(kNumCubeMapFaces);
    for (std::size_t i = 0; i < kNumCubeMapFaces; ++i) {
        auto result = loadImage(path / (kSideFilenames[i] + ".jpg"));
        if (!result) {
            return resultError("Failed to load image from path: {}", path.string());
        }
        cubeMapImages.push_back(result.unwrap());
    }

    return cubeMapImages;
}

Result<Image> loadImage(const std::filesystem::path& filePath, const int requestedChannels, const FlipAxis flip) {
    stbi_set_flip_vertically_on_load(flip == FlipAxis::Y);

    uint32_t elementSize = sizeof(uint8_t);

    void* dataPtr = nullptr;
    int32_t width = 0;
    int32_t height = 0;
    int32_t channelCount = 0;
    const std::string filePathString = filePath.string();
    if (filePath.extension().string() == ".hdr") {
        elementSize = sizeof(float);
        dataPtr = stbi_loadf(
            filePathString.c_str(), &width, &height, &channelCount, getStbComponentFormat(requestedChannels));
    } else {
        dataPtr =
            stbi_load(filePathString.c_str(), &width, &height, &channelCount, getStbComponentFormat(requestedChannels));
    }
    stbi_set_flip_vertically_on_load(false);

    if (!dataPtr) {
        return resultError("Failed to load image from {}. STB error: {}.", filePathString, stbi_failure_reason());
    }

    const auto imageWidth = static_cast<uint32_t>(width);
    const auto imageHeight = static_cast<uint32_t>(height);

    std::vector<uint8_t> pixelData(imageWidth * imageHeight * requestedChannels * elementSize);
    memcpy(pixelData.data(), dataPtr, pixelData.size());
    stbi_image_free(dataPtr);

    return Image(std::move(pixelData), imageWidth, imageHeight, requestedChannels, requestedChannels * elementSize);
}

Result<Image> loadImage(
    const std::span<const uint8_t> imageFileContent, const int requestedChannels, const FlipAxis flip) {
    stbi_set_flip_vertically_on_load(flip == FlipAxis::Y);

    uint32_t elementSize = sizeof(uint8_t);
    void* dataPtr = nullptr;
    int32_t width = 0;
    int32_t height = 0;
    int32_t channelCount = 0;
    dataPtr = stbi_load_from_memory(
        imageFileContent.data(),
        static_cast<int32_t>(imageFileContent.size()),
        &width,
        &height,
        &channelCount,
        getStbComponentFormat(requestedChannels));
    stbi_set_flip_vertically_on_load(false);

    if (!dataPtr) {
        return resultError("Failed to load image from memory. STB error: {}.", stbi_failure_reason());
    }

    const auto imageWidth = static_cast<uint32_t>(width);
    const auto imageHeight = static_cast<uint32_t>(height);

    std::vector<uint8_t> pixelData(imageWidth * imageHeight * requestedChannels * elementSize);
    memcpy(pixelData.data(), dataPtr, pixelData.size());
    stbi_image_free(dataPtr);

    return Image(std::move(pixelData), imageWidth, imageHeight, requestedChannels, requestedChannels * elementSize);
}

} // namespace crisp
