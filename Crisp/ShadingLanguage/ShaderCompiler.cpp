#include "ShaderCompiler.hpp"

#include <array>
#include <unordered_set>
#include <fstream>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace crisp
{
    namespace
    {
        static const std::unordered_set<std::string> shaderExtensions =
        {
            "vert", "frag", "tesc", "tese", "geom", "comp", "rgen", "rchit", "rmiss"
        };

        auto logger = spdlog::stderr_color_mt("ShaderCompiler");
    }

    void ShaderCompiler::compileDir(const std::filesystem::path& inputDir, const std::filesystem::path& outputDir)
    {
        logger->info("Processing and compiling shaders at path: {}", inputDir.string());
        logger->info("Spv Destination path: {}", outputDir.string());

        if (!std::filesystem::exists(outputDir))
        {
            if (!std::filesystem::create_directories(outputDir))
            {
                logger->error("Failed to create output directory {}", outputDir.string());
                return;
            }
        }

        std::array<char, 4096> lineBuffer;
        for (const auto& inputEntry : std::filesystem::directory_iterator(inputDir))
        {
            if (inputEntry.is_directory())
                continue;

            const std::filesystem::path inputPath = inputEntry.path();
            if (inputPath.extension() != ".glsl")
            {
                logger->warn("{} has no .glsl extension!", inputPath.string());
                continue;
            }

            // shader-name.<stage>.glsl is the file name format
            // First getting the stem and then its "extension" will give us the stage name
            const std::string shaderType = inputPath.stem().extension().string().substr(1); // Extension starts with a ., which we skip here
            if (!shaderExtensions.contains(shaderType))
            {
                logger->warn("{} is not a valid glsl shader type!", shaderType);
                continue;
            }

            const std::filesystem::path outputPath = outputDir / inputPath.filename().replace_extension("spv");

            const std::filesystem::file_time_type inputModifiedTs = inputEntry.last_write_time();
            const std::filesystem::file_time_type outputModifiedTs = std::filesystem::exists(outputPath) ?
                std::filesystem::last_write_time(outputPath) : std::filesystem::file_time_type{};

            if (inputModifiedTs > outputModifiedTs)
            {
                const std::filesystem::path tempOutputPath = outputDir / "temp.spv";

                logger->info("Compiling {}", inputPath.filename().string());
                std::string command = "glslangValidator.exe -V -o ";
                command += tempOutputPath.string();
                command += " -S ";
                command += shaderType;
                command += " ";
                command += inputPath.string();

                // Open a subprocess to compile this shader
                FILE* pipe = _popen(command.c_str(), "rt");
                if (!pipe)
                {
                    logger->error("Failed to open the pipe for {}", command);
                    _pclose(pipe);
                    continue;
                }

                // Read the pipe, typically the subprocess stdout
                lineBuffer.fill(0);
                while (fgets(lineBuffer.data(), lineBuffer.size(), pipe))
                {
                    const std::string_view view(lineBuffer.data(), std::strlen(lineBuffer.data()));
                    if (view.substr(0, 5) == "ERROR")
                        logger->error("{}", view);
                    else if (view.substr(0, 7) == "WARNING")
                        logger->warn("{}", view);
                }

                // Close the pipe and overwrite the output spv on success
                if (feof(pipe))
                {
                    const int retVal = _pclose(pipe);
                    if (retVal == 0)
                    {
                        // On success, we remove the older version of the compiled shader and rename the temp file appropriately
                        if (std::filesystem::exists(outputPath))
                            std::filesystem::remove(outputPath);

                        std::filesystem::rename(tempOutputPath, outputPath);
                    }
                    else
                    {
                        logger->error("Pipe process returned : {}", retVal);
                    }
                }
                else
                {
                    logger->error("Failed to read the pipe to the end.");
                }
            }
        }
    }
}
