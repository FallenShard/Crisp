#pragma once

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Math/BoundingBox.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Mesh/TriangleMeshView.hpp>
#include <Crisp/Mesh/VertexAttributeBuffer.hpp>

namespace crisp {
class TriangleMesh {
public:
    TriangleMesh() = default;
    TriangleMesh(
        std::vector<glm::vec3> positions,
        std::vector<glm::vec3> normals,
        std::vector<glm::vec2> texCoords,
        std::vector<glm::uvec3> faces);

    uint32_t getVertexCount() const;
    uint32_t getTriangleCount() const;
    uint32_t getIndexCount() const;

    const std::vector<glm::vec3>& getPositions() const;
    const std::vector<glm::vec3>& getNormals() const;
    const std::vector<glm::vec2>& getTexCoords() const;
    const std::vector<glm::vec4>& getTangents() const;
    const std::vector<glm::uvec3>& getTriangles() const;
    const VertexAttributeBuffer& getCustomAttribute(std::string_view attributeName) const;
    bool hasCustomAttribute(std::string_view attributeName) const;

    void setPositions(std::vector<glm::vec3>&& positions);
    void setNormals(std::vector<glm::vec3>&& normals);
    void setTexCoords(std::vector<glm::vec2>&& texCoords);
    void setTangents(std::vector<glm::vec4>&& tangents);
    void setTriangles(std::vector<glm::uvec3>&& triangles);
    void setCustomAttribute(std::string_view attributeName, VertexAttributeBuffer&& attributeBuffer);

    void computeVertexNormals();
    void computeTangentVectors();
    void computeBoundingBox();

    const BoundingBox3& getBoundingBox() const;

    const std::vector<TriangleMeshView>& getViews() const;
    void setViews(std::vector<TriangleMeshView>&& views);

    void normalizeToUnitBox();
    void transform(const glm::mat4& transform);

    void setMeshName(std::string&& meshName);
    std::string getMeshName() const;

    glm::vec3 calculateTriangleNormal(uint32_t triangleId) const;
    float calculateTriangleArea(uint32_t triangleId) const;

    glm::vec3 interpolatePosition(uint32_t triangleId, const glm::vec3& barycentric) const;
    glm::vec3 interpolateNormal(uint32_t triangleId, const glm::vec3& barycentric) const;
    glm::vec2 interpolateTexCoord(uint32_t triangleId, const glm::vec3& barycentric) const;

private:
    std::vector<glm::vec3> m_positions;
    std::vector<glm::vec3> m_normals;
    std::vector<glm::vec2> m_texCoords;
    std::vector<glm::vec4> m_tangents;
    std::vector<glm::vec4> m_colors;
    FlatStringHashMap<VertexAttributeBuffer> m_customAttributes;

    std::vector<glm::uvec3> m_triangles;
    std::vector<TriangleMeshView> m_views;
    std::string m_meshName;

    BoundingBox3 m_boundingBox;
};

std::vector<glm::vec3> computeVertexNormals(
    std::span<const glm::vec3> positions, std::span<const glm::uvec3> triangles);

InterleavedVertexBuffer interleaveVertexBuffers(
    const TriangleMesh& mesh, const std::vector<VertexAttributeDescriptor>& vertexAttribs, bool padToVec4);

std::vector<InterleavedVertexBuffer> interleaveVertexBuffers(
    const TriangleMesh& mesh, const std::vector<std::vector<VertexAttributeDescriptor>>& vertexAttribs, bool padToVec4);

} // namespace crisp