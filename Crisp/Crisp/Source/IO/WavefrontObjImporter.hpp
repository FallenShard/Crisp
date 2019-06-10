#pragma once

#include <CrispCore/Math/Headers.hpp>

#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

#include "Geometry/VertexAttributeDescriptor.hpp"

namespace crisp
{
    class TriangleMesh;

    class WavefrontObjImporter
    {
    public:
        WavefrontObjImporter(const std::filesystem::path& objFilePath);
        ~WavefrontObjImporter();

        bool import(const std::filesystem::path& objFilePath);

        std::unique_ptr<TriangleMesh> convertToTriangleMesh();
        void moveDataToTriangleMesh(TriangleMesh& triangleMesh, std::vector<VertexAttributeDescriptor> vertexAttribs);

        const std::filesystem::path& getPath() const;

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
    };
}