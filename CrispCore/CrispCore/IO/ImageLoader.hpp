#pragma once

#include <CrispCore/Image/Image.hpp>
#include <CrispCore/Result.hpp>

#include <filesystem>
#include <vector>

namespace crisp
{
    enum class FlipOnLoad
    {
        None,
        Y,
    };

    Result<Image> loadImage(const std::filesystem::path& filePath, int requestedChannels = 4, FlipOnLoad flip = FlipOnLoad::None);
}
