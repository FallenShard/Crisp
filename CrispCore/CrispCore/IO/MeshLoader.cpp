#include <CrispCore/IO/MeshLoader.hpp>
#include <CrispCore/IO/WavefrontObjReader.hpp>
#include <CrispCore/Logger.hpp>

namespace crisp
{
    namespace
    {
        auto logger = createLoggerMt("MeshReader");

        TriangleMesh convertToTriangleMesh(const std::filesystem::path& path, WavefrontObjMesh&& objMesh, std::vector<VertexAttributeDescriptor> vertexAttribs)
        {
            TriangleMesh mesh(std::move(objMesh.positions),
                std::move(objMesh.normals),
                std::move(objMesh.texCoords),
                std::move(objMesh.triangles),
                std::move(vertexAttribs));

            mesh.setMeshName(path.filename().stem().string());
            mesh.setViews(std::move(objMesh.views));
            return mesh;
        }
    }

    TriangleMesh loadMesh(const std::filesystem::path& path, const std::vector<VertexAttributeDescriptor>& vertexAttributes)
    {
        if (WavefrontObjReader::isWavefrontObjFile(path))
        {
            return convertToTriangleMesh(path, WavefrontObjReader().read(path), vertexAttributes);
        }
        /*else if (FbxImporter::isFbxFile(path))
        {
            FbxImporter importer(path);
            importer.moveDataToTriangleMesh(*this, m_interleavedFormat);
            return;
        }*/

        logger->critical("Failed to open an obj mesh at {}", path.string());
        throw std::runtime_error("Invalid path to a mesh");
    }
}