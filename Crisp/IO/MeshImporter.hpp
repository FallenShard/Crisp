#pragma once

#include <CrispCore/Math/Headers.hpp>

#include <filesystem>
#include <vector>

#include "Geometry/VertexAttributeDescriptor.hpp"

namespace crisp
{
    class TriangleMesh;

    class MeshImporter
    {
    public:
        MeshImporter();
        ~MeshImporter();

        bool import(const std::filesystem::path& path, TriangleMesh& mesh, std::vector<VertexAttributeDescriptor> vertexAttribs) const;

    private:

    };
}