#include <Crisp/Image/Io/ImageLoader.hpp>

#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4244) // conversion warnings
#include <stb/stb_image.h>
#pragma warning(pop)

#include <Crisp/Common/Format.hpp>

#include <string>

namespace crisp
{
auto getStbComponentFormat(const int numComponents)
{
    switch (numComponents)
    {
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

Result<Image> loadImage(const std::filesystem::path& filePath, const int requestedChannels, const FlipOnLoad flip)
{
    stbi_set_flip_vertically_on_load(flip == FlipOnLoad::Y);

    uint32_t elementSize = sizeof(uint8_t);
    void* dataPtr = nullptr;
    int32_t width = 0;
    int32_t height = 0;
    int32_t channelCount = 0;
    const std::string filePathString = filePath.string();
    if (filePath.extension().string() == ".hdr")
    {
        elementSize = sizeof(float);
        dataPtr = stbi_loadf(
            filePathString.c_str(), &width, &height, &channelCount, getStbComponentFormat(requestedChannels));
    }
    else
    {
        dataPtr =
            stbi_load(filePathString.c_str(), &width, &height, &channelCount, getStbComponentFormat(requestedChannels));
    }
    stbi_set_flip_vertically_on_load(false);

    if (!dataPtr)
    {
        return resultError("Failed to load image from {}. STB error: ", filePathString, stbi_failure_reason());
    }

    const uint32_t imageWidth = static_cast<uint32_t>(width);
    const uint32_t imageHeight = static_cast<uint32_t>(height);

    std::vector<uint8_t> pixelData(imageWidth * imageHeight * requestedChannels * elementSize);
    memcpy(pixelData.data(), dataPtr, pixelData.size());
    stbi_image_free(dataPtr);

    return Image(std::move(pixelData), imageWidth, imageHeight, requestedChannels, requestedChannels * elementSize);
}

Result<Image> loadImage(
    const std::span<const uint8_t> imageFileContent, const int requestedChannels, const FlipOnLoad flip)
{
    stbi_set_flip_vertically_on_load(flip == FlipOnLoad::Y);

    uint32_t elementSize = sizeof(uint8_t);
    void* dataPtr = nullptr;
    int32_t width = 0;
    int32_t height = 0;
    int32_t channelCount = 0;
    /*const std::string filePathString = filePath.string();
    if (filePath.extension().string() == ".hdr")
    {
        elementSize = sizeof(float);
        dataPtr = stbi_loadf(
            filePathString.c_str(), &width, &height, &channelCount, getStbComponentFormat(requestedChannels));
    }
    else
    {*/
    dataPtr = stbi_load_from_memory(
        imageFileContent.data(),
        static_cast<int32_t>(imageFileContent.size()),
        &width,
        &height,
        &channelCount,
        getStbComponentFormat(requestedChannels));
    //}
    stbi_set_flip_vertically_on_load(false);

    if (!dataPtr)
    {
        return resultError("Failed to load image from memory. STB error: ", stbi_failure_reason());
    }

    const uint32_t imageWidth = static_cast<uint32_t>(width);
    const uint32_t imageHeight = static_cast<uint32_t>(height);

    std::vector<uint8_t> pixelData(imageWidth * imageHeight * requestedChannels * elementSize);
    for (uint32_t i = 0; i < imageHeight; ++i)
    {
        for (uint32_t j = 0; j < imageWidth; ++j)
        {
            const std::size_t copySize = std::min(channelCount, requestedChannels) * elementSize;
            uint8_t* dstPtr = &pixelData[(imageWidth * i + j) * requestedChannels * elementSize];
            const uint8_t* srcPtr =
                &(static_cast<uint8_t*>(dataPtr)[(imageWidth * i + j) * channelCount * elementSize]);
            std::memcpy(dstPtr, srcPtr, copySize);
        }
    }
    stbi_image_free(dataPtr);

    return Image(std::move(pixelData), imageWidth, imageHeight, requestedChannels, requestedChannels * elementSize);
}

} // namespace crisp