#include <CrispCore/IO/ImageLoader.hpp>

#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 4244) // conversion warnings
#include <stb/stb_image.h>
#pragma warning(pop)

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <string>

namespace crisp
{
    auto getStbComponentFormat(int numComponents)
    {
        switch (numComponents)
        {
        case 0:  return STBI_default;
        case 1:  return STBI_grey;
        case 2:  return STBI_grey_alpha;
        case 3:  return STBI_rgb;
        case 4:  return STBI_rgb_alpha;
        default: return STBI_default;
        }
    }

    Image loadImage(const std::filesystem::path& filePath, const int requestedChannels, const FlipOnLoad flip)
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
            dataPtr = stbi_loadf(filePathString.c_str(), &width, &height, &channelCount, getStbComponentFormat(requestedChannels));
        }
        else
        {
            dataPtr = stbi_load(filePathString.c_str(), &width, &height, &channelCount, getStbComponentFormat(requestedChannels));
        }
        stbi_set_flip_vertically_on_load(false);

        if (!dataPtr)
        {
            throw std::runtime_error(fmt::format("Failed to load image {}.", filePathString));
        }

        const uint32_t imageWidth = static_cast<uint32_t>(width);
        const uint32_t imageHeight = static_cast<uint32_t>(height);


        std::vector<uint8_t> pixelData(imageWidth * imageHeight * channelCount * elementSize);
        memcpy(pixelData.data(), dataPtr, pixelData.size());
        stbi_image_free(dataPtr);

        return Image(std::move(pixelData), imageWidth, imageHeight, channelCount, channelCount * elementSize);
    }
}