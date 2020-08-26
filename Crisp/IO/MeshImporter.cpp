#include "MeshImporter.hpp"

#include "WavefrontObjImporter.hpp"

namespace crisp
{
    MeshImporter::MeshImporter()
    {
    }

    MeshImporter::~MeshImporter()
    {
    }

    bool MeshImporter::import(const std::filesystem::path& path, TriangleMesh& triangleMesh, std::vector<VertexAttributeDescriptor> vertexAttribs) const
    {
        if (WavefrontObjImporter::isWavefrontObjFile(path))
        {
            WavefrontObjImporter objImporter(path);
            objImporter.moveDataToTriangleMesh(triangleMesh, vertexAttribs);
            return true;
        }

        return false;
    }
}