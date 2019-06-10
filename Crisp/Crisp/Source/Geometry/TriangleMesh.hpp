#pragma once

#include "Geometry/VertexAttributeDescriptor.hpp"
#include "Geometry/VertexAttributeBuffer.hpp"
#include "Geometry/VertexAttributeTraits.hpp"
#include "Geometry/GeometryPart.hpp"

#include <CrispCore/Math/Headers.hpp>

#include <filesystem>
#include <vector>
#include <unordered_map>
#include <variant>

namespace crisp
{
    class TriangleMesh
    {
    public:
        TriangleMesh();
        TriangleMesh(const std::filesystem::path& filePath);
        TriangleMesh(const std::filesystem::path& filePath, const std::vector<VertexAttributeDescriptor>& vertexAttributes);
        TriangleMesh(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals,
            const std::vector<glm::vec2>& texCoords, const std::vector<glm::uvec3>& faces, const std::vector<VertexAttributeDescriptor>& vertexAttributes);
        ~TriangleMesh();

        void setMeshName(std::string&& meshName);
        std::string getMeshName() const;

        void setPositions(std::vector<glm::vec3>&& positions);
        void setNormals(std::vector<glm::vec3>&& normals);
        void setTexCoords(std::vector<glm::vec2>&& texCoords);
        void setCustomAttribute(const std::string& id, VertexAttributeBuffer&& attributeBuffer);

        void setFaces(std::vector<glm::uvec3>&& faces);
        void setParts(std::vector<GeometryPart>&& parts);

        const std::vector<glm::vec3>& getPositions() const;
        const std::vector<glm::vec3>& getNormals() const;
        const std::vector<glm::vec2>& getTexCoords() const;
        const VertexAttributeBuffer& getCustomAttribute(const std::string& id) const;

        const std::vector<glm::uvec3>& getFaces() const;
        uint32_t getNumFaces() const;
        uint32_t getNumIndices() const;
        uint32_t getNumVertices() const;

        InterleavedVertexBuffer interleaveAttributes() const;

        static std::vector<glm::vec3> computeVertexNormals(const std::vector<glm::vec3>& positions, const std::vector<glm::uvec3>& faces);
        void computeTangentVectors();
        void computeVertexNormals();

        const std::vector<GeometryPart> getGeometryParts() const;

    private:
        std::vector<glm::vec3> m_positions;
        std::vector<glm::vec3> m_normals;
        std::vector<glm::vec2> m_texCoords;
        std::vector<glm::vec3> m_tangents;
        std::vector<glm::vec3> m_bitangents;
        std::unordered_map<std::string, VertexAttributeBuffer> m_customAttributes;

        std::vector<glm::uvec3> m_faces;

        std::vector<GeometryPart> m_parts;

        std::vector<VertexAttributeDescriptor> m_interleavedFormat;

        std::string m_meshName;
    };
}