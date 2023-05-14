#pragma once

#include <Crisp/Core/Result.hpp>
#include <Crisp/Image/Image.hpp>

#include <filesystem>
#include <span>

namespace crisp
{
enum class FlipOnLoad
{
    None,
    Y,
};

std::vector<Image> loadCubeMapFacesFromHCrossImage(
    const std::filesystem::path& path, FlipOnLoad flipOnLoad = FlipOnLoad::None);

Result<Image> loadImage(
    const std::filesystem::path& filePath, int requestedChannels = 4, FlipOnLoad flip = FlipOnLoad::None);
Result<Image> loadImage(
    const std::span<const uint8_t> imageFileContent, int requestedChannels = 4, FlipOnLoad flip = FlipOnLoad::None);

} // namespace crisp
