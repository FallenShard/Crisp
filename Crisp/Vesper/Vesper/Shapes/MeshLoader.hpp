#pragma once

#include <vector>
#include <fstream>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

namespace vesper
{
    struct Transform;

    class MeshLoader
    {
    public:
        bool load(std::string fileName, 
            std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals, 
            std::vector<glm::vec2>& texCoords, std::vector<glm::uvec3>& faces, 
            const Transform& transform) const;

    private:
        void loadWavefrontObj(std::ifstream& file, 
            std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals, 
            std::vector<glm::vec2>& texCoords, std::vector<glm::uvec3>& faces) const;

        bool loadModelCache(std::string fileName,
            std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals,
            std::vector<glm::vec2>& texCoords, std::vector<glm::uvec3>& faces,
            const Transform& transform) const;

        void createModelCache(std::string fileName,
            std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals,
            std::vector<glm::vec2>& texCoords, std::vector<glm::uvec3>& faces) const;

        void applyTransform(std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals, const Transform& transform) const;
    };
}