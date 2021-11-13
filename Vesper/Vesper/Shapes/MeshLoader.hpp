#pragma once

#include <CrispCore/Math/Headers.hpp>

#include <filesystem>
#include <vector>


namespace crisp
{
    class MeshLoader
    {
    public:
        bool load(std::filesystem::path path,
            std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals,
            std::vector<glm::vec2>& texCoords, std::vector<glm::uvec3>& faces) const;

    private:
        bool loadModelCache(std::string fileName,
            std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals,
            std::vector<glm::vec2>& texCoords, std::vector<glm::uvec3>& faces) const;

        void createModelCache(std::string fileName,
            const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals,
            const std::vector<glm::vec2>& texCoords, const std::vector<glm::uvec3>& faces) const;
    };
}