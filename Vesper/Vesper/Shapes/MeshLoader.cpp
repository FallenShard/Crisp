#include <Vesper/Shapes/MeshLoader.hpp>

#include <Crisp/IO/WavefrontObjReader.hpp>

#include <spdlog/spdlog.h>

#include <fstream>

namespace crisp
{
bool MeshLoader::load(
    std::filesystem::path path,
    std::vector<glm::vec3>& positions,
    std::vector<glm::vec3>& normals,
    std::vector<glm::vec2>& texCoords,
    std::vector<glm::uvec3>& faces) const
{
    std::string ext = path.extension().string();

    auto cachedModelPath = path.remove_filename() / "cache" / path.filename();

    if (loadModelCache(cachedModelPath.string(), positions, normals, texCoords, faces))
    {
        spdlog::info("Loading cached version of Wavefront Obj mesh: {}", cachedModelPath.filename().string());
        return true;
    }

    if (WavefrontObjReader::isWavefrontObjFile(path))
    {
        spdlog::info("Loading Wavefront Obj mesh: {}", path.filename().string());
        auto objMesh = WavefrontObjReader().read(path);
        positions = std::move(objMesh.positions);
        normals = std::move(objMesh.normals);
        texCoords = std::move(objMesh.texCoords);
        faces = std::move(objMesh.triangles);
        createModelCache(cachedModelPath.string(), positions, normals, texCoords, faces);
        return true;
    }

    return false;
}

bool MeshLoader::loadModelCache(
    std::string fileName,
    std::vector<glm::vec3>& positions,
    std::vector<glm::vec3>& normals,
    std::vector<glm::vec2>& texCoords,
    std::vector<glm::uvec3>& faces) const
{
    std::ifstream binaryFile(fileName + ".cmf", std::ios::in | std::ios::binary);
    if (binaryFile.fail())
        return false;

    std::size_t numVertices;
    binaryFile.read(reinterpret_cast<char*>(&numVertices), sizeof(std::size_t));
    positions.resize(numVertices);

    std::size_t numNormals;
    binaryFile.read(reinterpret_cast<char*>(&numNormals), sizeof(std::size_t));
    normals.resize(numNormals);

    std::size_t numTexCoords;
    binaryFile.read(reinterpret_cast<char*>(&numTexCoords), sizeof(std::size_t));
    texCoords.resize(numTexCoords);

    std::size_t numFaces;
    binaryFile.read(reinterpret_cast<char*>(&numFaces), sizeof(std::size_t));
    faces.resize(numFaces);

    binaryFile.read(reinterpret_cast<char*>(positions.data()), numVertices * sizeof(glm::vec3));
    binaryFile.read(reinterpret_cast<char*>(normals.data()), numNormals * sizeof(glm::vec3));
    binaryFile.read(reinterpret_cast<char*>(texCoords.data()), numTexCoords * sizeof(glm::vec2));
    binaryFile.read(reinterpret_cast<char*>(faces.data()), numFaces * sizeof(glm::uvec3));

    return true;
}

void MeshLoader::createModelCache(
    std::string fileName,
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& normals,
    const std::vector<glm::vec2>& texCoords,
    const std::vector<glm::uvec3>& faces) const
{
    std::ofstream binaryFile(fileName + ".cmf", std::ios::out | std::ios::binary);

    const std::size_t numVertices = positions.size();
    binaryFile.write(reinterpret_cast<const char*>(&numVertices), sizeof(std::size_t));

    const std::size_t numNormals = normals.size();
    binaryFile.write(reinterpret_cast<const char*>(&numNormals), sizeof(std::size_t));

    const std::size_t numTexCoords = texCoords.size();
    binaryFile.write(reinterpret_cast<const char*>(&numTexCoords), sizeof(std::size_t));

    const std::size_t numFaces = faces.size();
    binaryFile.write(reinterpret_cast<const char*>(&numFaces), sizeof(std::size_t));

    binaryFile.write(reinterpret_cast<const char*>(positions.data()), numVertices * sizeof(glm::vec3));
    binaryFile.write(reinterpret_cast<const char*>(normals.data()), numNormals * sizeof(glm::vec3));
    binaryFile.write(reinterpret_cast<const char*>(texCoords.data()), numTexCoords * sizeof(glm::vec2));
    binaryFile.write(reinterpret_cast<const char*>(faces.data()), numFaces * sizeof(glm::uvec3));
}
} // namespace crisp