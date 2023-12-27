#pragma once

#include <filesystem>
#include <span>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Image/Image.hpp>
#include <Crisp/Image/Io/Common.hpp>

namespace crisp {

std::vector<Image> loadCubeMapFacesFromHCrossImage(const std::filesystem::path& path, FlipAxis flip = FlipAxis::None);

Result<Image> loadImage(
    const std::filesystem::path& filePath, int requestedChannels = 4, FlipAxis flip = FlipAxis::None);
Result<Image> loadImage(
    std::span<const uint8_t> imageFileContent, int requestedChannels = 4, FlipAxis flip = FlipAxis::None);

} // namespace crisp
