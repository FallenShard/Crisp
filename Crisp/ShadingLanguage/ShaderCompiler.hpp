#pragma once

#include <filesystem>

namespace crisp
{
    class ShaderCompiler
    {
    public:
        void compileDir(const std::filesystem::path& inputDir, const std::filesystem::path& outputDir);

    private:
    };
}