#include <Crisp/Common/Logger.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Mesh/Io/WavefrontObjReader.hpp>

namespace crisp
{
namespace
{
auto logger = createLoggerMt("MeshReader");

TriangleMesh convertToTriangleMesh(
    const std::filesystem::path& path, WavefrontObjMesh&& objMesh, std::vector<VertexAttributeDescriptor> vertexAttribs)
{
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
    const std::filesystem::path& path, const std::vector<VertexAttributeDescriptor>& vertexAttributes)
{
    if (isWavefrontObjFile(path))
    {
        return convertToTriangleMesh(path, readWavefrontObj(path), vertexAttributes);
    }
    /*else if (FbxImporter::isFbxFile(path))
    {
        FbxImporter importer(path);
        importer.moveDataToTriangleMesh(*this, m_interleavedFormat);
        return;
    }*/

    return resultError("Failed to open an obj mesh at {}", path.string());
}
} // namespace crisp