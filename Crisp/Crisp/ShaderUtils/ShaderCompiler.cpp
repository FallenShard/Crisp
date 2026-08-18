#include <Crisp/ShaderUtils/ShaderCompiler.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Core/UniqueTemporaryFile.hpp>
#include <Crisp/Io/FileUtils.hpp>
#include <Crisp/ShaderUtils/ShaderType.hpp>

namespace crisp {
namespace {
const FlatHashSet<std::filesystem::path> kSupportedExtensions{".glsl", ".slang"};
constexpr std::size_t kMaxShaderCompilationThreads = 8;

const FlatStringHashMap<std::string> kGlslangToSlangStageMap = {
    {"frag", "fragment"},
};

const FlatStringHashMap<EShLanguage> kGlslangStageMap = {
    {"vert", EShLangVertex},
    {"frag", EShLangFragment},
    {"tesc", EShLangTessControl},
    {"tese", EShLangTessEvaluation},
    {"geom", EShLangGeometry},
    {"comp", EShLangCompute},
    {"rgen", EShLangRayGen},
    {"rchit", EShLangClosestHit},
    {"rahit", EShLangAnyHit},
    {"rits", EShLangIntersect},
    {"rmiss", EShLangMiss},
    {"rcall", EShLangCallable},
    {"mesh", EShLangMesh},
    {"task", EShLangTask},
};

std::string getShaderTypeFromPath(const std::filesystem::path& shaderPath) {
    const auto sourceExtension = shaderPath.extension();
    const auto stageExtension =
        sourceExtension == ".glsl" || sourceExtension == ".slang"
            ? shaderPath.stem().extension().string()
            : sourceExtension.string();
    return stageExtension.starts_with('.') ? stageExtension.substr(1) : std::string{};
}

CRISP_MAKE_LOGGER_MT("ShaderCompiler");

constexpr std::string_view kSlangExecutable{"slangc"};

class GlslangProcessScope {
public:
    GlslangProcessScope()
        : m_isInitialized(glslang::InitializeProcess()) {}

    GlslangProcessScope(const GlslangProcessScope&) = delete;
    GlslangProcessScope& operator=(const GlslangProcessScope&) = delete;
    GlslangProcessScope(GlslangProcessScope&&) noexcept = delete;
    GlslangProcessScope& operator=(GlslangProcessScope&&) noexcept = delete;

    ~GlslangProcessScope() {
        if (m_isInitialized) {
            glslang::FinalizeProcess();
        }
    }

    bool isInitialized() const {
        return m_isInitialized;
    }

private:
    bool m_isInitialized;
};

class RelativeFileIncluder final : public glslang::TShader::Includer {
public:
    IncludeResult* includeLocal(
        const char* headerName, const char* includerName, std::size_t /*inclusionDepth*/) override {
        const std::filesystem::path relativePath(headerName);
        if (relativePath.empty() || relativePath.has_root_path()) {
            return nullptr;
        }

        const auto resolvedPath = (std::filesystem::path(includerName).parent_path() / relativePath).lexically_normal();
        auto sourceResult = readBinaryFile(resolvedPath);
        if (!sourceResult) {
            return nullptr;
        }

        auto includeData = std::make_unique<IncludeData>(IncludeData{
            .path = resolvedPath.string(),
            .source = std::move(sourceResult).extract(),
        });

        auto result = std::make_unique<IncludeResult>(
            includeData->path, includeData->source.data(), includeData->source.size(), includeData.get());
        includeData.release(); // NOLINT
        return result.release();
    }

    IncludeResult* includeSystem(
        const char* /*headerName*/, const char* /*includerName*/, std::size_t /*inclusionDepth*/) override {
        return nullptr;
    }

    void releaseInclude(IncludeResult* result) override {
        if (result != nullptr) {
            delete static_cast<IncludeData*>(result->userData);
            delete result;
        }
    }

private:
    struct IncludeData {
        std::string path;
        std::vector<char> source;
    };
};

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

Result<ShaderTimestamp> getCachedLastWriteTime(
    const std::filesystem::path& path, ShaderTimestampCache& timestampCache, std::mutex& timestampCacheMutex) {
    const auto normalizedPath = std::filesystem::absolute(path).lexically_normal();
    const std::scoped_lock lock(timestampCacheMutex);
    if (const auto iter = timestampCache.find(normalizedPath); iter != timestampCache.end()) {
        return iter->second;
    }

    return timestampCache.emplace(normalizedPath, getLastWriteTime(normalizedPath)).first->second;
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
    ShaderTimestampCache& timestampCache,
    std::mutex& timestampCacheMutex) {
    CRISP_TRY(const auto outputModifiedTime, getCachedLastWriteTime(outputPath, timestampCache, timestampCacheMutex));
    if (!outputModifiedTime) {
        return true; // Output file doesn't exist, so we need to recompile.
    }

    FlatHashSet<std::filesystem::path> shaderSources;
    if (!collectShaderSources(inputPath, shaderSources)) {
        return true; // One of the input files doesn't exist, so we need to recompile/report error.
    }

    for (const auto& shaderSource : shaderSources) {
        CRISP_TRY(
            const auto sourceModifiedTime, getCachedLastWriteTime(shaderSource, timestampCache, timestampCacheMutex));
        if (!sourceModifiedTime || *sourceModifiedTime > *outputModifiedTime) {
            return true;
        }
    }

    return false;
}

Result<> compileSlangShader(
    const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, const std::string_view shaderType) {
    const auto stage = kGlslangToSlangStageMap.find(shaderType);
    if (stage == kGlslangToSlangStageMap.end()) {
        return resultError("Shader stage {} is not supported by the Slang compiler", shaderType);
    }

    const auto absoluteInputPath = std::filesystem::absolute(inputPath).lexically_normal();
    const auto command = fmt::format(
        "{} -o {} -stage {} {}",
        kSlangExecutable,
        quoteShellArgument(outputPath.string()),
        quoteShellArgument(stage->second),
        quoteShellArgument(absoluteInputPath.string()));
    CRISP_TRY(
        const auto processOutput, runProcess(command), "Failed to run the compiler for shader {}", inputPath.string());
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

Result<> compileShader(
    const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, const std::string_view shaderType) {
    if (inputPath.extension() == ".glsl") {
        return compileGlslShader(inputPath, outputPath, shaderType);
    }
    return compileSlangShader(inputPath, outputPath, shaderType);
}

struct ShaderCompilationTask {
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    std::string shaderType;
};

enum class ShaderCompilationOutcome : uint8_t {
    Recompiled,
    Skipped,
};

Result<ShaderCompilationOutcome> executeShaderCompilationTask(
    const ShaderCompilationTask& task, ShaderTimestampCache& timestampCache, std::mutex& timestampCacheMutex) {
    CRISP_TRY(
        const auto shouldRecompile,
        shaderNeedsRecompilation(task.inputPath, task.outputPath, timestampCache, timestampCacheMutex),
        "Failed to query timestamps for shader {}",
        task.inputPath.string());
    if (!shouldRecompile) {
        return ShaderCompilationOutcome::Skipped;
    }

    const auto compilationStartTime = std::chrono::steady_clock::now();
    CRISP_TRY(compileShader(task.inputPath, task.outputPath, task.shaderType));
    const auto compilationDuration =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - compilationStartTime);
    CRISP_LOGI("Compiled | {:<48} | {:>9.2f} ms", task.inputPath.filename().string(), compilationDuration.count());
    return ShaderCompilationOutcome::Recompiled;
}
} // namespace

Result<> compileGlslShader(
    const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, const std::string_view shaderType) {
    CRISP_TRY(const auto spirv, compileGlslShader(inputPath, shaderType));
    return writeBinaryFile(outputPath, std::as_bytes(std::span(spirv)));
}

Result<std::vector<uint32_t>> compileGlslShader(
    const std::filesystem::path& inputPath, const std::string_view shaderType) {
    if (inputPath.extension() == ".slang") {
        return resultError("Cannot compile Slang source {} with glslang", inputPath.string());
    }

    const std::string inferredShaderType = shaderType.empty() ? getShaderTypeFromPath(inputPath) : std::string{};
    const std::string_view resolvedShaderType = shaderType.empty() ? inferredShaderType : shaderType;
    if (!isGlslShaderExtension(resolvedShaderType)) {
        return resultError("{} does not have a supported GLSL shader stage", inputPath.string());
    }

    static const GlslangProcessScope glslangProcess;
    if (!glslangProcess.isInitialized()) {
        return resultError("Failed to initialize glslang");
    }

    const auto stageIter = kGlslangStageMap.find(resolvedShaderType);
    if (stageIter == kGlslangStageMap.end()) {
        return resultError("Shader stage {} is not supported by glslang", resolvedShaderType);
    }
    const EShLanguage stage = stageIter->second;

    CRISP_TRY(auto source, fileToString(inputPath));
    if (source.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return resultError("Shader source {} is too large for glslang", inputPath.string());
    }

    const auto absoluteInputPath = std::filesystem::absolute(inputPath).lexically_normal().string();
    const char* sourcePointer = source.data();
    const int sourceLength = static_cast<int>(source.size());
    const char* sourceName = absoluteInputPath.c_str();

    glslang::TShader shader(stage);
    shader.setStringsWithLengthsAndNames(&sourcePointer, &sourceLength, &sourceName, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_4);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

    constexpr auto kMessages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
    RelativeFileIncluder includer;
    if (!shader.parse(GetDefaultResources(), 450, false, kMessages, includer)) {
        return resultError(
            "Failed to compile shader {}:\n{}{}", inputPath.string(), shader.getInfoLog(), shader.getInfoDebugLog());
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(kMessages)) {
        return resultError(
            "Failed to link shader {}:\n{}{}", inputPath.string(), program.getInfoLog(), program.getInfoDebugLog());
    }

    const auto* intermediate = program.getIntermediate(stage);
    if (intermediate == nullptr) {
        return resultError("glslang produced no intermediate representation for {}", inputPath.string());
    }

    std::vector<uint32_t> spirv;
    spv::SpvBuildLogger buildLogger;
    glslang::GlslangToSpv(*intermediate, spirv, &buildLogger);
    const auto buildMessages = buildLogger.getAllMessages();
    if (!buildMessages.empty()) {
        CRISP_LOGD("SPIR-V generation output for {}:\n{}", inputPath.filename().string(), buildMessages);
    }
    if (spirv.empty()) {
        return resultError("glslang produced an empty SPIR-V module for {}", inputPath.string());
    }
    return spirv;
}

Result<ShaderCompilationStats> recompileShaderDir(
    const std::filesystem::path& inputDir, const std::filesystem::path& outputDir) {
    if (!std::filesystem::exists(inputDir)) {
        return resultError("Specified shader input directory {} doesn't exist!", inputDir.string());
    }
    const auto processingStartTime = std::chrono::steady_clock::now();

    CRISP_LOGD("Processing and compiling shaders from: {}", inputDir.string());
    CRISP_LOGD("Saving .spv modules in: {}", outputDir.string());

    if (!std::filesystem::exists(outputDir)) {
        if (!std::filesystem::create_directories(outputDir)) {
            return resultError("Failed to create output directory {}", outputDir.string());
        }
    }

    std::vector<ShaderCompilationTask> tasks;
    FlatHashSet<std::filesystem::path> scheduledOutputPaths;
    for (const auto& inputEntry : std::filesystem::recursive_directory_iterator(inputDir)) {
        const std::filesystem::path& inputPath = inputEntry.path();
        const auto relativePath = inputPath.lexically_relative(inputDir);
        if (relativePath.empty() || relativePath.is_absolute() || *relativePath.begin() == "..") {
            return resultError(
                "Failed to determine the path of {} relative to {}", inputPath.string(), inputDir.string());
        }

        if (inputEntry.is_directory()) {
            const auto outputDirectory = (outputDir / relativePath).lexically_normal();
            std::error_code directoryError;
            std::filesystem::create_directories(outputDirectory, directoryError);
            if (directoryError) {
                return resultError(
                    "Failed to create output directory {}: {}", outputDirectory.string(), directoryError.message());
            }
            continue;
        }

        if (inputPath.string().ends_with("part.glsl")) {
            continue;
        }

        if (!kSupportedExtensions.contains(inputPath.extension())) {
            CRISP_LOGW("{} has an unsupported shader source extension!", inputPath.string());
            continue;
        }

        // shader-name.<stage>.<language> is the file name format
        // First getting the stem and then its "extension" will give us the stage name
        const std::string shaderType = getShaderTypeFromPath(inputPath);
        if (!isGlslShaderExtension(shaderType)) {
            CRISP_LOGW("{} is not a valid shader stage!", shaderType);
            continue;
        }

        auto relativeOutputPath = relativePath;
        relativeOutputPath.replace_extension("spv");
        const auto outputPath = (outputDir / relativeOutputPath).lexically_normal();
        if (!scheduledOutputPaths.emplace(outputPath).second) {
            return resultError("Multiple shader sources produce the same output path: {}", outputPath.string());
        }

        tasks.push_back(
            ShaderCompilationTask{
                .inputPath = inputPath,
                .outputPath = outputPath,
                .shaderType = shaderType,
            });
    }

    ShaderTimestampCache timestampCache;
    std::mutex timestampCacheMutex;
    std::vector<std::optional<Result<ShaderCompilationOutcome>>> taskResults(tasks.size());
    if (!tasks.empty()) {
        const auto hardwareThreadCount = std::max(1u, std::thread::hardware_concurrency());
        const auto workerCount = std::min({
            tasks.size(),
            static_cast<std::size_t>(hardwareThreadCount),
            kMaxShaderCompilationThreads,
        });
        CRISP_LOGD("Processing {} shaders on {} compilation threads.", tasks.size(), workerCount);

        std::atomic_size_t nextTaskIndex{0};
        std::atomic_bool compilationFailed{false};
        {
            std::vector<std::jthread> workers;
            workers.reserve(workerCount);
            for (std::size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
                workers.emplace_back([&] {
                    while (!compilationFailed.load(std::memory_order_relaxed)) {
                        const auto taskIndex = nextTaskIndex.fetch_add(1, std::memory_order_relaxed);
                        if (taskIndex >= tasks.size()) {
                            return;
                        }

                        auto taskResult =
                            executeShaderCompilationTask(tasks[taskIndex], timestampCache, timestampCacheMutex);
                        const bool succeeded = taskResult.hasValue();
                        taskResults[taskIndex].emplace(std::move(taskResult));
                        if (!succeeded) {
                            compilationFailed.store(true, std::memory_order_relaxed);
                        }
                    }
                });
            }
        }
    }

    ShaderCompilationStats stats;
    for (auto& taskResult : taskResults) {
        if (!taskResult) {
            continue;
        }
        if (!*taskResult) {
            return std::unexpected<std::string>(std::move(*taskResult).getError());
        }

        if (**taskResult == ShaderCompilationOutcome::Recompiled) {
            ++stats.recompiledShaderCount;
        } else {
            ++stats.skippedShaderCount;
        }
    }
    const auto processingDuration =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - processingStartTime);
    CRISP_LOGI(
        "Processed {} shaders in {:.2f} ms: {} recompiled, {} skipped.",
        tasks.size(),
        processingDuration.count(),
        stats.recompiledShaderCount,
        stats.skippedShaderCount);
    return stats;
}

} // namespace crisp
