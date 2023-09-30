#include "OpenEXRReader.hpp"

#pragma warning(push)
#pragma warning(disable : 4018)
#pragma warning(disable : 4706)
#pragma warning(disable : 4389)
#pragma warning(disable : 4100)
#pragma warning(disable : 6387)
#pragma warning(disable : 6386)
#pragma warning(disable : 6385)
#pragma warning(disable : 6001)
#pragma warning(disable : 6011)
#pragma warning(disable : 26451)
#pragma warning(disable : 26819)
#pragma warning(disable : 26812)
#pragma warning(disable : 26495)
#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>
#pragma warning(pop)
#include <Crisp/Core/Logger.hpp>

#include <iostream>
#include <vector>

namespace crisp {
namespace {
auto logger = spdlog::stdout_color_st("OpenEXRReader");
}

OpenEXRReader::OpenEXRReader() {}

OpenEXRReader::~OpenEXRReader() {}

bool OpenEXRReader::read(
    const std::filesystem::path& imagePath, std::vector<float>& rgb, uint32_t& width, uint32_t& height) {
    const std::string pathString = imagePath.string();

    // Read EXR version
    EXRVersion version;
    if (ParseEXRVersionFromFile(&version, pathString.c_str()) != TINYEXR_SUCCESS) {
        logger->error("Could not parse version from EXR file: {}", pathString);
        return false;
    }

    // Make sure it is not multipart
    if (version.multipart) {
        logger->error("Multipart EXR files are not supported: ", pathString);
        return false;
    }

    // Read EXR header
    EXRHeader header;
    InitEXRHeader(&header);
    const char* err = nullptr;
    if (ParseEXRHeaderFromFile(&header, &version, pathString.c_str(), &err) != TINYEXR_SUCCESS) {
        logger->error("Failed to parse EXR header from file {}: {}", pathString, err);
        return false;
    }

    // Request half-floats to be converted to 32bit floats
    for (int i = 0; i < header.num_channels; ++i) {
        if (header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
            header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
        }
    }

    // Load image
    EXRImage image;
    InitEXRImage(&image);
    if (LoadEXRImageFromFile(&image, &header, pathString.c_str(), &err) != TINYEXR_SUCCESS) {
        logger->error("Failed to load EXR image from file {}: {}", pathString, err);
        FreeEXRHeader(&header);
        return false;
    }

    width = static_cast<uint32_t>(image.width);
    height = static_cast<uint32_t>(image.height);
    rgb.reserve(width * height * image.num_channels);
    for (int i = 0; i < image.height; i++) {
        for (int j = 0; j < image.width; j++) {
            const uint32_t pixelIdx = static_cast<uint32_t>(image.width * i + j);

            for (int k = 0; k < image.num_channels; ++k) {
                rgb.push_back(std::max(0.0f, reinterpret_cast<float**>(image.images)[k][pixelIdx]));
            }
        }
    }

    FreeEXRHeader(&header);
    FreeEXRImage(&image);
    return true;
}
} // namespace crisp