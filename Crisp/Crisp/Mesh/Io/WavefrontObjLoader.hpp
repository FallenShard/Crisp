#pragma once

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Mesh/TriangleMeshView.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace crisp {
struct WavefrontObjMaterial {
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

struct WavefrontObjMesh {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::uvec3> triangles;

    std::vector<TriangleMeshView> views;

    FlatHashMap<std::string, WavefrontObjMaterial> materials;
};

bool isWavefrontObjFile(const std::filesystem::path& path);
WavefrontObjMesh loadWavefrontObj(const std::filesystem::path& objFilePath);

} // namespace crisp
