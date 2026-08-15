#include <Crisp/ShaderUtils/ShaderCompiler.hpp>

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>
#include <string_view>
#include <system_error>

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Io/FileUtils.hpp>
#include <Crisp/ShaderUtils/ShaderType.hpp>

namespace crisp {
namespace {
const FlatHashSet<std::filesystem::path> kSupportedExtensions{".glsl", ".slang"};

const FlatStringHashMap<std::string> kGlslangToSlangStageMap = {
    {"frag", "fragment"},
    {},
};

CRISP_MAKE_LOGGER_MT("ShaderCompiler");

using ShaderTimestamp = std::optional<std::filesystem::file_time_type>;
using ShaderTimestampCache = FlatHashMap<std::filesystem::path, Result<ShaderTimestamp>>;

Result<ShaderTimestamp> getCachedLastWriteTime(const std::filesystem::path& path, ShaderTimestampCache& timestampCache) {
    const auto normalizedPath = std::filesystem::absolute(path).lexically_normal();
    if (const auto iter = timestampCache.find(normalizedPath); iter != timestampCache.end()) {
        return iter->second;
    }

    CRISP_TRY(const auto cachedTimestamp, getLastWriteTime(normalizedPath));
    return timestampCache.emplace(normalizedPath, cachedTimestamp).first->second;
}

std::optional<std::filesystem::path> parseRelativeInclude(const std::string_view line) {
    constexpr std::string_view kIncludeKeyword{"include"};

    auto cursor = line.find_first_not_of(" \t");
    if (cursor == std::string_view::npos || line[cursor] != '#') {
        return std::nullopt;
    }
    cursor = line.find_first_not_of(" \t", cursor + 1);
    if (cursor == std::string_view::npos || !line.substr(cursor).starts_with(kIncludeKeyword)) {
        return std::nullopt;
    }
    cursor += kIncludeKeyword.size();
    if (cursor < line.size() && line[cursor] != ' ' && line[cursor] != '\t' && line[cursor] != '"') {
        return std::nullopt;
    }

    const auto openingQuote = line.find('"', cursor);
    if (openingQuote == std::string_view::npos) {
        return std::nullopt;
    }
    const auto closingQuote = line.find('"', openingQuote + 1);
    if (closingQuote == std::string_view::npos) {
        return std::nullopt;
    }

    std::filesystem::path includePath{line.substr(openingQuote + 1, closingQuote - openingQuote - 1)};
    if (includePath.empty() || includePath.has_root_path()) {
        return std::nullopt;
    }
    return includePath;
}

bool collectShaderSources(const std::filesystem::path& sourcePath, FlatHashSet<std::filesystem::path>& dependencies) {
    const auto normalizedPath = std::filesystem::absolute(sourcePath).lexically_normal();
    if (dependencies.contains(normalizedPath)) {
        return true;
    }
    dependencies.insert(normalizedPath);

    std::ifstream source(normalizedPath);
    if (!source) {
        return false;
    }

    bool allDependenciesExist = true;
    std::string line;
    while (std::getline(source, line)) {
        if (const auto includePath = parseRelativeInclude(line)) {
            const auto dependencyPath = (normalizedPath.parent_path() / *includePath).lexically_normal();
            allDependenciesExist &= collectShaderSources(dependencyPath, dependencies);
        }
    }
    return allDependenciesExist;
}

Result<bool> shaderNeedsRecompilation(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputPath,
    ShaderTimestampCache& timestampCache) {
    CRISP_TRY(const auto outputModifiedTime, getCachedLastWriteTime(outputPath, timestampCache));
    if (!outputModifiedTime) {
        return true; // Output file doesn't exist, so we need to recompile.
    }

    FlatHashSet<std::filesystem::path> shaderSources;
    if (!collectShaderSources(inputPath, shaderSources)) {
        return true; // One of the input files doesn't exist, so we need to recompile/report error.
    }

    for (const auto& shaderSource : shaderSources) {
        CRISP_TRY(const auto sourceModifiedTime, getCachedLastWriteTime(shaderSource, timestampCache));
        if (!sourceModifiedTime || *sourceModifiedTime > *outputModifiedTime) {
            return true;
        }
    }

    return false;
}
} // namespace

Result<ShaderCompilationStats> recompileShaderDir(
    const std::filesystem::path& inputDir, const std::filesystem::path& outputDir) {
    if (!std::filesystem::exists(inputDir)) {
        return resultError("Specified shader input directory {} doesn't exist!", inputDir.string());
    }

    CRISP_LOGD("Processing and compiling shaders from: {}", inputDir.string());
    CRISP_LOGD("Saving .spv modules in: {}", outputDir.string());

    if (!std::filesystem::exists(outputDir)) {
        if (!std::filesystem::create_directories(outputDir)) {
            return resultError("Failed to create output directory {}", outputDir.string());
        }
    }

    ShaderCompilationStats stats;
    ShaderTimestampCache timestampCache;
    std::array<char, 4096> lineBuffer; // NOLINT
    for (const auto& inputEntry : std::filesystem::recursive_directory_iterator(inputDir)) {
        if (inputEntry.is_directory()) {
            continue;
        }

        const std::filesystem::path& inputPath = inputEntry.path();
        if (inputPath.string().ends_with("part.glsl")) {
            continue;
        }

        if (!kSupportedExtensions.contains(inputPath.extension())) {
            CRISP_LOGW("{} has no .glsl extension!", inputPath.string());
            continue;
        }

        // shader-name.<stage>.glsl is the file name format
        // First getting the stem and then its "extension" will give us the stage name
        const std::string shaderType = inputPath.stem().extension().string().substr(1); // Extension starts with a .,
                                                                                        // which we skip here
        if (!isGlslShaderExtension(shaderType)) {
            CRISP_LOGW("{} is not a valid glsl shader type!", shaderType);
            continue;
        }

        // Output file is shader-name.<stage>.spv
        const std::filesystem::path outputPath = outputDir / inputPath.filename().replace_extension("spv");
        CRISP_TRY(
            const auto shouldRecompile,
            shaderNeedsRecompilation(inputPath, outputPath, timestampCache),
            "Failed to query timestamps for shader {}",
            inputPath.string());
        if (shouldRecompile) {
            const std::filesystem::path tempOutputPath = outputDir / "temp.spv";
            const std::filesystem::path absoluteInputPath = std::filesystem::absolute(inputPath).lexically_normal();

            CRISP_LOGI("Compiling {}", inputPath.filename().string());

            const std::string command =
                inputPath.extension() == ".glsl"
                    ? fmt::format(
                          R"(glslangValidator.exe --target-env vulkan1.3 -o "{}" -S {} "{}")",
                          tempOutputPath.string(),
                          shaderType,
                          absoluteInputPath.string())
                    : fmt::format(
                          R"(slangc.exe -o "{}" -stage {} "{}")",
                          tempOutputPath.string(),
                          kGlslangToSlangStageMap.find(shaderType)->second,
                          absoluteInputPath.string());

            // Open a subprocess to compile this shader
            FILE* pipe = _popen(command.c_str(), "rt");
            if (!pipe) {
                return resultError("Failed to launch shader compiler command: {}", command);
            }

            // Read the pipe, typically the subprocess stdout
            lineBuffer.fill(0);
            bool compilerReportedError{false};
            while (fgets(lineBuffer.data(), static_cast<int>(lineBuffer.size()), pipe)) {
                const std::string_view view(lineBuffer.data(), std::strlen(lineBuffer.data()));
                if (view.starts_with("ERROR")) {
                    CRISP_LOGE("{}", view);
                    compilerReportedError = true;
                } else if (view.starts_with("WARNING")) {
                    CRISP_LOGW("{}", view);
                }
            }

            const bool pipeReadFailed = ferror(pipe) != 0;
            const int returnCode = _pclose(pipe);
            if (pipeReadFailed) {
                std::error_code ignoredError;
                std::filesystem::remove(tempOutputPath, ignoredError);
                return resultError("Failed to read compiler output for shader {}", inputPath.string());
            }
            if (returnCode != 0 || compilerReportedError) {
                std::error_code ignoredError;
                std::filesystem::remove(tempOutputPath, ignoredError);
                return resultError(
                    "Failed to compile shader {} (compiler exit code {})", inputPath.string(), returnCode);
            }

            std::error_code fileError;
            std::filesystem::remove(outputPath, fileError);
            if (fileError) {
                return resultError("Failed to remove old shader {}: {}", outputPath.string(), fileError.message());
            }
            std::filesystem::rename(tempOutputPath, outputPath, fileError);
            if (fileError) {
                return resultError("Failed to move compiled shader to {}: {}", outputPath.string(), fileError.message());
            }
            ++stats.recompiledShaderCount;
        } else {
            ++stats.skippedShaderCount;
        }
    }
    CRISP_LOGI(
        "{} shaders recompiled, {} shaders skipped.", stats.recompiledShaderCount, stats.skippedShaderCount);
    return stats;
}

} // namespace crisp
