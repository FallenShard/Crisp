#include "OpenEXRWriter.hpp"

#include <Crisp/Core/Logger.hpp>

#include <tinyexr/tinyexr.h>

#include <iostream>
#include <vector>

namespace crisp
{
namespace
{
auto logger = spdlog::stdout_color_st("OpenEXRWriter");
}

OpenEXRWriter::OpenEXRWriter() {}

OpenEXRWriter::~OpenEXRWriter() {}

void OpenEXRWriter::write(
    const std::string& fileName, const float* rgba, unsigned int width, unsigned int height, bool flipYAxis)
{
    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage image;
    InitEXRImage(&image);

    // 4 = RGBA channels
    image.num_channels = 4;

    std::vector<std::vector<float>> images(image.num_channels, std::vector<float>(width * height, 0.0f));

    for (unsigned int y = 0; y < height; y++)
    {
        for (unsigned int x = 0; x < width; x++)
        {
            unsigned int rowIdx = flipYAxis ? height - 1 - y : y;
            unsigned int colIdx = x;

            for (int c = 0; c < image.num_channels; c++)
            {
                const int channelIdx = image.num_channels - 1 - c;
                // OpenEXR expects (A)BGR
                images[c][y * width + colIdx] = rgba[image.num_channels * (rowIdx * width + colIdx) + channelIdx];
            }
        }
    }

    std::vector<float*> imagePtrs(image.num_channels, nullptr);
    for (int c = 0; c < image.num_channels; c++)
    {
        imagePtrs[c] = images[c].data();
    }

    image.images = reinterpret_cast<unsigned char**>(imagePtrs.data());
    image.width = width;
    image.height = height;

    header.num_channels = image.num_channels;
    header.channels = (EXRChannelInfo*)malloc(sizeof(EXRChannelInfo) * header.num_channels);
    header.channels[0].name[0] = 'A';
    header.channels[0].name[1] = '\0';
    header.channels[1].name[0] = 'B';
    header.channels[1].name[1] = '\0';
    header.channels[2].name[0] = 'G';
    header.channels[2].name[1] = '\0';
    header.channels[3].name[0] = 'R';
    header.channels[3].name[1] = '\0';

    header.pixel_types = (int*)malloc(sizeof(int) * header.num_channels);
    header.requested_pixel_types = (int*)malloc(sizeof(int) * header.num_channels);
    for (int i = 0; i < header.num_channels; i++)
    {
        header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;           // pixel type of input image
        header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // pixel type of output image to be stored in .EXR
    }

    const char* err = nullptr;
    if (SaveEXRImageToFile(&image, &header, fileName.c_str(), &err) != TINYEXR_SUCCESS)
    {
        logger->error("Failed to save EXR image into file {}: {}", fileName, err);
        FreeEXRHeader(&header);
        FreeEXRImage(&image);
        return;
    }

    FreeEXRHeader(&header);
    // FreeEXRImage(&image);
}
} // namespace crisp