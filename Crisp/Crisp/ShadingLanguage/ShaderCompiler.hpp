#pragma once

#include <CrispCore/Result.hpp>

#include <filesystem>
#include <string>

namespace crisp
{
    void recompileShaderDir(const std::filesystem::path& inputDir, const std::filesystem::path& outputDir);

    Result<std::string> preprocessGlslSource(const std::filesystem::path& inputPath);
}