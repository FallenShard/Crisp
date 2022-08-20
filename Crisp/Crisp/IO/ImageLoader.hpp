#pragma once

#include <Crisp/Common/Result.hpp>
#include <Crisp/Image/Image.hpp>


#include <filesystem>
#include <span>
#include <vector>

namespace crisp
{
enum class FlipOnLoad
{
    None,
    Y,
};

Result<Image> loadImage(
    const std::filesystem::path& filePath, int requestedChannels = 4, FlipOnLoad flip = FlipOnLoad::None);
Result<Image> loadImage(
    const std::span<const uint8_t> imageFileContent, int requestedChannels = 4, FlipOnLoad flip = FlipOnLoad::None);
} // namespace crisp
