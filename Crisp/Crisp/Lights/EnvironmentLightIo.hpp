#pragma once

#include <filesystem>

#include <Crisp/Lights/EnvironmentLight.hpp>

namespace crisp {
Result<ImageBasedLightingData> loadImageBasedLightingData(const std::filesystem::path& environmentMapDir);
} // namespace crisp
