#pragma once

#include <CrispCore/Math/Headers.hpp>

#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

#include <Geometry/VertexAttributeDescriptor.hpp>

namespace crisp
{
    class TriangleMesh;

    class FbxImporter
    {
    public:
        FbxImporter(const std::filesystem::path& filePath);
        ~FbxImporter();

        bool import(const std::filesystem::path& filePath);

        std::unique_ptr<TriangleMesh> convertToTriangleMesh();


        static bool isFbxFile(const std::filesystem::path& filePath);

        void moveDataToTriangleMesh(TriangleMesh& triangleMesh, std::vector<VertexAttributeDescriptor> vertexAttribs);

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
    };

}
