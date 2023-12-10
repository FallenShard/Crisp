#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Image/Io/Common.hpp>

namespace crisp {

struct ExrImageData {
    std::vector<float> pixelData;
    uint32_t width{0};
    uint32_t height{0};
    uint32_t channelCount{0};
};

Result<ExrImageData> loadExr(const std::filesystem::path& imagePath);

Result<> saveExr(
    const std::filesystem::path& outputPath,
    std::span<const float> hdrPixelData,
    uint32_t width,
    uint32_t height,
    FlipAxis flipAxis = FlipAxis::None);

} // namespace crisp
