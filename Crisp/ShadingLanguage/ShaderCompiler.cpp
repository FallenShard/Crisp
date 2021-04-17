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

        void replaceAll(std::string& string, const std::string& key, const std::string& replacement)
        {
            std::size_t pos = 0;
            while ((pos = string.find(key, pos)) != std::string::npos)
            {
                string.replace(pos, key.size(), replacement);
                pos += replacement.size();
            }
        }

        auto logger = spdlog::stderr_color_mt("ShaderCompiler");
    }

    void ShaderCompiler::compileDir(const std::filesystem::path& inputDir, const std::filesystem::path& outputDir)
    {
        logger->info("Processing and compiling shaders at path: {}", inputDir.string());
        logger->info("Spv Destination path: {}", outputDir.string());

        std::filesystem::create_directories(outputDir);

        std::array<char, 4096> lineBuffer;
        for (const auto& inputEntry : std::filesystem::directory_iterator(inputDir))
        {
            if (inputEntry.is_directory())
                continue;

            std::filesystem::path inputPath = inputEntry.path();

            if (inputPath.extension().string() != ".glsl")
            {
                logger->warn("{} has no .glsl extension!\n", inputPath.string());
                continue;
            }

            // shader-name.<stage>.glsl is the file name format
            auto shaderType = inputPath.stem().extension().string().substr(1); // Extension starts with a ., which we skip here
            if (!shaderExtensions.count(shaderType))
            {
                logger->warn("{} is not a valid glsl shader type!\n", shaderType);
                continue;
            }

            std::filesystem::path outputPath = outputDir / inputPath.filename().replace_extension("spv");

            std::filesystem::file_time_type inputModifiedTs = inputEntry.last_write_time();
            std::filesystem::file_time_type outputModifiedTs;
            if (std::filesystem::exists(outputPath))
                outputModifiedTs = std::filesystem::last_write_time(outputPath);

            if (inputModifiedTs > outputModifiedTs)
            {
                if (std::filesystem::exists(outputPath))
                    std::filesystem::remove(outputPath);

                logger->info("Compiling {}", inputPath.filename().string());
                std::string command = "glslangValidator.exe -V -o ";
                command += outputPath.string();
                command += " -S ";
                command += shaderType;
                command += " ";
                command += inputPath.string();

                // Open a subprocess to compile this shader
                FILE* pipe = _popen(command.c_str(), "rt");
                if (!pipe)
                {
                    _pclose(pipe);
                    continue;
                }

                // Read the pipe, typically the subprocess stdout
                lineBuffer.fill(0);
                while (fgets(lineBuffer.data(), lineBuffer.size(), pipe))
                {
                    std::size_t len = std::strlen(lineBuffer.data());
                    std::string_view view(lineBuffer.data(), len);
                    if (view.substr(0, 5) == "ERROR")
                        logger->error("{}", view);
                    else if (view.substr(0, 7) == "WARNING")
                        logger->warn("{}", view);
                }

                /* Close pipe and print return value of pPipe. */
                if (feof(pipe))
                {
                    int retVal = _pclose(pipe);
                    if (retVal == 0)
                        logger->info("Success");
                    else
                        logger->error("Pipe process returned : {}", retVal);
                }
                else
                {
                    logger->error("Failed to read the pipe to the end.");
                }
            }
            //else
            //{
            //    std::cout << "Skipping " << inputPath << " as it is newer.\n";
            //    std::cout << inputModified.time_since_epoch().count() << '\n';
            //    std::cout << outputModified.time_since_epoch().count() << '\n';
            //}
        }
    }
}
