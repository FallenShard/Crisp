#pragma once

#include <Crisp/Lights/EnvironmentLight.hpp>

#include <filesystem>

namespace crisp {
Result<ImageBasedLightingData> loadImageBasedLightingData(const std::filesystem::path& path);
} // namespace crisp
