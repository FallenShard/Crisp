#include <Crisp/Mesh/Io/MeshLoader.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Mesh/Io/WavefrontObjLoader.hpp>

namespace crisp {
namespace {
auto logger = createLoggerMt("MeshLoader");

TriangleMesh convertToTriangleMesh(
    const std::filesystem::path& path,
    WavefrontObjMesh&& objMesh,
    std::vector<VertexAttributeDescriptor> vertexAttribs) {
    TriangleMesh mesh(
        std::move(objMesh.positions),
        std::move(objMesh.normals),
        std::move(objMesh.texCoords),
        std::move(objMesh.triangles),
        std::move(vertexAttribs));

    mesh.setMeshName(path.filename().stem().string());
    mesh.setViews(std::move(objMesh.views));
    return mesh;
}
} // namespace

Result<TriangleMesh> loadTriangleMesh(
    const std::filesystem::path& path, const std::vector<VertexAttributeDescriptor>& vertexAttributes) {
    if (isWavefrontObjFile(path)) {
        return convertToTriangleMesh(path, loadWavefrontObj(path), vertexAttributes);
    }

    return resultError("Failed to open an obj mesh at {}", path.string());
}

Result<MeshAndMaterial> loadTriangleMeshAndMaterial(
    const std::filesystem::path& path, const std::vector<VertexAttributeDescriptor>& vertexAttributes) {
    if (isWavefrontObjFile(path)) {
        auto objMesh = loadWavefrontObj(path);
        auto materials = std::move(objMesh.materials);
        return MeshAndMaterial{convertToTriangleMesh(path, std::move(objMesh), vertexAttributes), std::move(materials)};
    }

    return resultError("Failed to open an obj mesh at {}", path.string());
}
} // namespace crisp