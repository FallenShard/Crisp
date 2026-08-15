#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Crisp/Core/UniqueTemporaryFile.hpp>
#include <Crisp/Io/FileUtils.hpp>
#include <Crisp/ShaderUtils/ShaderCompiler.hpp>

namespace crisp {

class TestShaderMap {
public:
    explicit TestShaderMap(std::initializer_list<std::filesystem::path> shaderPaths) {
        for (const auto& shaderPath : shaderPaths) {
            const auto shaderName = shaderPath.filename().string();
            if (shaderName.empty()) {
                throw std::invalid_argument("Test shader path has no filename");
            }

            const bool inserted = m_shaders.try_emplace(shaderName, shaderPath).second;
            if (!inserted) {
                throw std::invalid_argument("Duplicate test shader filename: " + shaderName);
            }
        }
    }

    [[nodiscard]] std::span<const uint32_t> getSpirv(const std::string_view shaderName) const {
        const std::scoped_lock lock(m_mutex);
        return getOrCompileSpirv(findShader(shaderName));
    }

    [[nodiscard]] const std::filesystem::path& getSpirvPath(const std::string_view shaderName) const {
        const std::scoped_lock lock(m_mutex);
        const auto& shader = findShader(shaderName);
        const auto spirv = getOrCompileSpirv(shader);
        if (!shader.spirvFile) {
            shader.spirvFile.emplace("spv", "test-shader-");
            auto result = writeBinaryFile(shader.spirvFile->getPath(), std::as_bytes(spirv));
            if (!result.isValid()) {
                shader.spirvFile.reset();
                throw std::runtime_error(std::move(result).getError());
            }
        }
        return shader.spirvFile->getPath();
    }

private:
    struct Shader {
        explicit Shader(std::filesystem::path path)
            : sourcePath(std::move(path)) {}

        std::filesystem::path sourcePath;
        mutable std::optional<std::vector<uint32_t>> spirv;
        mutable std::optional<UniqueTemporaryFile> spirvFile;
    };

    const Shader& findShader(const std::string_view shaderName) const {
        const auto shaderIter = m_shaders.find(shaderName);
        if (shaderIter == m_shaders.end()) {
            throw std::out_of_range("Test shader is not registered: " + std::string(shaderName));
        }
        return shaderIter->second;
    }

    static std::span<const uint32_t> getOrCompileSpirv(const Shader& shader) {
        if (!shader.spirv) {
            auto result = compileGlslShader(shader.sourcePath);
            if (!result) {
                throw std::runtime_error(std::move(result).getError());
            }
            shader.spirv.emplace(std::move(result).extract());
        }
        return std::span<const uint32_t>(*shader.spirv);
    }

    std::map<std::string, Shader, std::less<>> m_shaders;
    mutable std::mutex m_mutex;
};

} // namespace crisp
