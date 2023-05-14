#pragma once

#include <Crisp/Core/Result.hpp>

#include <filesystem>
#include <string>

namespace crisp
{
struct GlslSourceFile
{
    std::string sourceCode;
    std::filesystem::file_time_type lastModifiedRecursive;
};

void recompileShaderDir(const std::filesystem::path& inputDir, const std::filesystem::path& outputDir);

Result<GlslSourceFile> preprocessGlslSource(const std::filesystem::path& inputPath);
} // namespace crisp