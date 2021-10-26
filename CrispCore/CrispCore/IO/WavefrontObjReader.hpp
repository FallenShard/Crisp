#pragma once

#include <CrispCore/Mesh/TriangleMeshView.hpp>
#include <CrispCore/Math/Headers.hpp>
#include <CrispCore/RobinHood.hpp>

#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

namespace crisp
{
    struct WavefrontObjMaterial
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

    struct WavefrontObjMesh
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        std::vector<glm::uvec3> triangles;

        std::vector<TriangleMeshView> views;

        robin_hood::unordered_flat_map<std::string, WavefrontObjMaterial> materials;
    };

    class WavefrontObjReader
    {
    public:
        WavefrontObjMesh read(const std::filesystem::path& objFilePath);

        static bool isWavefrontObjFile(const std::filesystem::path& path);
    };
}
