#pragma once

#include <filesystem>
#include <vector>
#include <fstream>
#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
    class MeshLoader
    {
    public:
        bool load(std::filesystem::path path,
            std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals,
            std::vector<glm::vec2>& texCoords, std::vector<glm::uvec3>& faces) const;

    private:
        void loadWavefrontObj(std::ifstream& file,
            std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals,
            std::vector<glm::vec2>& texCoords, std::vector<glm::uvec3>& faces) const;

        bool loadModelCache(std::string fileName,
            std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals,
            std::vector<glm::vec2>& texCoords, std::vector<glm::uvec3>& faces) const;

        void createModelCache(std::string fileName,
            std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals,
            std::vector<glm::vec2>& texCoords, std::vector<glm::uvec3>& faces) const;
    };
}