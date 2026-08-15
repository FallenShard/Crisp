#include <Crisp/ShaderUtils/ShaderCompiler.hpp>

#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Core/UniqueTemporaryFile.hpp>
#include <Crisp/Io/FileUtils.hpp>
#include <Crisp/ShaderUtils/ShaderType.hpp>

namespace crisp {
namespace {
const FlatHashSet<std::filesystem::path> kSupportedExtensions{".glsl", ".slang"};

const FlatStringHashMap<std::string> kGlslangToSlangStageMap = {
    {"frag", "fragment"},
};

CRISP_MAKE_LOGGER_MT("ShaderCompiler");

constexpr std::string_view kGlslangExecutable{CRISP_GLSLANG_VALIDATOR_PATH};
constexpr std::string_view kSlangExecutable{"slangc"};

struct ProcessOutput {
    int exitCode;
    std::string output;
};

std::string quoteShellArgument(const std::string_view argument) {
    return fmt::format("\"{}\"", argument);
}

Result<ProcessOutput> runProcess(std::string command) {
    const UniqueTemporaryFile processOutputFile("log", "shader-process-");
    command += fmt::format(" > {} 2>&1", quoteShellArgument(processOutputFile.getPath().string()));

    const int exitCode = std::system(command.c_str());
    if (exitCode == -1) {
        return resultError("Failed to launch process: {}", command);
    }
    CRISP_TRY(auto output, fileToString(processOutputFile.getPath()));
    return ProcessOutput{.exitCode = exitCode, .output = std::move(output)};
}

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

Result<> compileShader(
    const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, const std::string_view shaderType) {
    const auto absoluteInputPath = std::filesystem::absolute(inputPath).lexically_normal();
    std::string command;
    if (inputPath.extension() == ".glsl") {
        command = fmt::format(
            "{} --target-env vulkan1.3 -o {} -S {} {}",
            kGlslangExecutable,
            quoteShellArgument(outputPath.string()),
            quoteShellArgument(shaderType),
            quoteShellArgument(absoluteInputPath.string()));
    } else {
        const auto stage = kGlslangToSlangStageMap.find(shaderType);
        if (stage == kGlslangToSlangStageMap.end()) {
            return resultError("Shader stage {} is not supported by the Slang compiler", shaderType);
        }
        command = fmt::format(
            "{} -o {} -stage {} {}",
            kSlangExecutable,
            quoteShellArgument(outputPath.string()),
            quoteShellArgument(stage->second),
            quoteShellArgument(absoluteInputPath.string()));
    }

    CRISP_LOGI("Compiling {}", inputPath.filename().string());
    CRISP_TRY(
        const auto& processOutput,
        runProcess(std::move(command)),
        "Failed to run the compiler for shader {}",
        inputPath.string());
    if (processOutput.exitCode != 0) {
        return resultError(
            "Failed to compile shader {} (compiler exit code {}):\n{}",
            inputPath.string(),
            processOutput.exitCode,
            processOutput.output);
    }
    if (!processOutput.output.empty()) {
        CRISP_LOGD("Compiler output for {}:\n{}", inputPath.filename().string(), processOutput.output);
    }

    std::error_code fileError;
    const auto outputSize = std::filesystem::file_size(outputPath, fileError);
    if (fileError || outputSize == 0) {
        return resultError(
            "Compiler did not produce a valid SPIR-V file for {}{}",
            inputPath.string(),
            fileError ? fmt::format(": {}", fileError.message()) : std::string{});
    }
    return kResultSuccess;
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
            CRISP_TRY(compileShader(inputPath, outputPath, shaderType));
            ++stats.recompiledShaderCount;
        } else {
            ++stats.skippedShaderCount;
        }
    }
    CRISP_LOGI("{} shaders recompiled, {} shaders skipped.", stats.recompiledShaderCount, stats.skippedShaderCount);
    return stats;
}

} // namespace crisp
