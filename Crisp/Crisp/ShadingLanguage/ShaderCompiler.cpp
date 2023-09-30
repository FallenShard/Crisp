#include <Crisp/ShadingLanguage/ShaderCompiler.hpp>

#include <Crisp/ShadingLanguage/ShaderType.hpp>

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/IO/FileUtils.hpp>

#include <Crisp/Core/Logger.hpp>

#include <array>
#include <fstream>

namespace crisp {
namespace {
const std::filesystem::path GlslExtension = ".glsl";

auto logger = spdlog::stderr_color_mt("ShaderCompiler");
} // namespace

void recompileShaderDir(const std::filesystem::path& inputDir, const std::filesystem::path& outputDir) {
    if (!std::filesystem::exists(inputDir)) {
        logger->error("Specified input directory {} doesn't exist!", inputDir.string());
        return;
    }

    logger->info("Processing and compiling shaders from: {}", inputDir.string());
    logger->info("Saving .spv modules in: {}", outputDir.string());

    if (!std::filesystem::exists(outputDir)) {
        if (!std::filesystem::create_directories(outputDir)) {
            logger->error("Failed to create output directory {}", outputDir.string());
            return;
        }
    }

    uint32_t shadersSkipped{0};
    uint32_t shadersRecompiled{0};
    std::array<char, 4096> lineBuffer; // NOLINT
    for (const auto& inputEntry : std::filesystem::directory_iterator(inputDir)) {
        if (inputEntry.is_directory()) {
            continue;
        }

        const std::filesystem::path& inputPath = inputEntry.path();
        if (inputPath.extension() != GlslExtension) {
            logger->warn("{} has no .glsl extension!", inputPath.string());
            continue;
        }

        // shader-name.<stage>.glsl is the file name format
        // First getting the stem and then its "extension" will give us the stage name
        const std::string shaderType =
            inputPath.stem().extension().string().substr(1); // Extension starts with a ., which we skip here
        if (!isGlslShaderExtension(shaderType)) {
            logger->warn("{} is not a valid glsl shader type!", shaderType);
            continue;
        }

        // Output file is shader-name.<stage>.spv
        const std::filesystem::path outputPath = outputDir / inputPath.filename().replace_extension("spv");

        auto maybeGlslSource{preprocessGlslSource(inputPath)};
        if (!maybeGlslSource.hasValue()) {
            logger->error(maybeGlslSource.getError());
            continue;
        }
        const auto glslSource{std::move(maybeGlslSource).unwrap()};
        const std::filesystem::file_time_type outputModifiedTs =
            std::filesystem::exists(outputPath)
                ? std::filesystem::last_write_time(outputPath)
                : std::filesystem::file_time_type{};
        if (glslSource.lastModifiedRecursive > outputModifiedTs) {
            const std::filesystem::path tempOutputPath = outputDir / "temp.spv";
            const std::filesystem::path tempInputPath = inputDir / "temp.glsl";

            logger->info("Compiling {}", inputPath.filename().string());

            stringToFile(tempInputPath, glslSource.sourceCode).unwrap();

            const std::string command = fmt::format(
                "glslangValidator.exe --target-env vulkan1.3 -o {} -S {} {}",
                tempOutputPath.string(),
                shaderType,
                tempInputPath.string());

            // Open a subprocess to compile this shader
            FILE* pipe = _popen(command.c_str(), "rt");
            if (!pipe) {
                logger->error("Failed to open the pipe for {}", command);
                _pclose(pipe);
                continue;
            }

            // Read the pipe, typically the subprocess stdout
            lineBuffer.fill(0);
            uint32_t errorCount{0};
            while (fgets(lineBuffer.data(), static_cast<int>(lineBuffer.size()), pipe)) {
                const std::string_view view(lineBuffer.data(), std::strlen(lineBuffer.data()));
                if (view.substr(0, 5) == "ERROR") {
                    logger->error("{}", view);
                    ++errorCount;
                } else if (view.substr(0, 7) == "WARNING") {
                    logger->warn("{}", view);
                }
            }
            if (errorCount > 0) {
                logger->critical("Encountered errors while compiling: {}", inputPath.stem().string());
                std::filesystem::remove(tempInputPath);
                std::abort();
            }

            // Close the pipe and overwrite the output spv on success
            if (feof(pipe)) {
                const int retVal = _pclose(pipe);
                if (retVal == 0) {
                    // On success, we remove the older version of the compiled shader and rename the temp file
                    // appropriately
                    if (std::filesystem::exists(outputPath)) {
                        std::filesystem::remove(outputPath);
                    }

                    std::filesystem::rename(tempOutputPath, outputPath);
                } else {
                    logger->error("Pipe process returned: {}", retVal);
                }
            } else {
                logger->error("Failed to read the pipe to the end.");
            }

            std::filesystem::remove(tempInputPath);

            ++shadersRecompiled;
        } else {
            ++shadersSkipped;
        }
    }
    logger->info("{} shaders recompiled, {} shaders skipped.", shadersRecompiled, shadersSkipped);
}

Result<GlslSourceFile> preprocessGlslSource(const std::filesystem::path& inputPath) {
    constexpr std::string_view includeDirective("#include");
    constexpr std::size_t trimLeft = 2;  // space + \"
    constexpr std::size_t trimRight = 1; // \"

    std::stringstream preprocessed;

    std::size_t lineIdx = 0;
    std::string line;
    std::ifstream inputFile(inputPath);

    GlslSourceFile result{};
    result.lastModifiedRecursive = std::filesystem::last_write_time(inputPath);
    while (std::getline(inputFile, line)) {
        if (line.starts_with(includeDirective)) {
            const std::string relativeIncludePath = line.substr(includeDirective.size() + trimLeft);
            const std::filesystem::path includeFilePath =
                inputPath.parent_path() / relativeIncludePath.substr(0, relativeIncludePath.size() - trimRight);
            if (!std::filesystem::exists(includeFilePath)) {
                return resultError(
                    "Invalid include path {} at line {} of {}!", relativeIncludePath, lineIdx, inputPath.string());
            }
            preprocessed << fileToString(includeFilePath).unwrap() << '\n';

            const auto includeWriteTime{std::filesystem::last_write_time(includeFilePath)};
            if (includeWriteTime > result.lastModifiedRecursive) {
                result.lastModifiedRecursive = includeWriteTime;
            }
        } else {
            preprocessed << line << '\n';
        }
        ++lineIdx;
    }

    result.sourceCode = preprocessed.str();
    return result;
}
} // namespace crisp
