#pragma once

#include <CrispCore/Math/Headers.hpp>

#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

namespace crisp
{
    class WavefrontObjReader
    {
    public:
        struct Material
        {
            glm::vec3 ambient;
            glm::vec3 diffuse;
            glm::vec3 specular;

            float ns;
            float ni;
            float d;
            int illumination;

            std::string albedoMap;
            std::string normalMap;
            std::string specularMap;
        };

        WavefrontObjReader(const std::filesystem::path& objFilePath);
        ~WavefrontObjReader();

        const std::filesystem::path& getPath() const;

        const std::vector<glm::vec3>& getPositions() const;
        const std::vector<glm::vec3>& getNormals() const;
        const std::vector<glm::vec2>& getTexCoords() const;

        const std::vector<glm::uvec3>& getTriangles() const;

        bool read(const std::filesystem::path& objFilePath);

        static bool isWavefrontObjFile(const std::filesystem::path& path);

    private:
        std::filesystem::path m_path;

        std::vector<glm::vec3> m_positions;
        std::vector<glm::vec3> m_normals;
        std::vector<glm::vec2> m_texCoords;

        std::vector<glm::uvec3> m_triangles;

        struct MeshPart
        {
            std::string tag;
            uint32_t offset;
            uint32_t count;
        };
        std::vector<MeshPart> m_meshParts;
        std::unordered_map<std::string, Material> m_materials;
    };
}
