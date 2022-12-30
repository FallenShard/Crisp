#pragma once

#include <Crisp/Common/HashMap.hpp>
#include <Crisp/Math/BoundingBox.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Mesh/TriangleMeshView.hpp>
#include <Crisp/Mesh/VertexAttributeBuffer.hpp>

#include <vector>

namespace crisp
{
class TriangleMesh
{
public:
    TriangleMesh() = default;
    TriangleMesh(
        std::vector<glm::vec3> positions,
        std::vector<glm::vec3> normals,
        std::vector<glm::vec2> texCoords,
        std::vector<glm::uvec3> faces,
        std::vector<VertexAttributeDescriptor> vertexAttributes);

    TriangleMesh(const TriangleMesh& mesh) = delete;
    TriangleMesh(TriangleMesh&& mesh) noexcept = default;
    TriangleMesh& operator=(const TriangleMesh& mesh) = delete;
    TriangleMesh& operator=(TriangleMesh&& mesh) noexcept = default;

    const std::vector<glm::uvec3>& getFaces() const;
    uint32_t getFaceCount() const;
    uint32_t getIndexCount() const;
    uint32_t getVertexCount() const;

    std::vector<InterleavedVertexBuffer> interleave(
        const std::vector<std::vector<VertexAttributeDescriptor>>& vertexAttribs, bool padToVec4) const;

    static std::vector<glm::vec3> computeVertexNormals(
        const std::vector<glm::vec3>& positions, const std::vector<glm::uvec3>& faces);
    void computeVertexNormals();
    void computeTangentVectors();
    void computeBoundingBox();

    const std::vector<TriangleMeshView>& getViews() const;
    void normalizeToUnitBox();
    void transform(const glm::mat4& transform);

    void setMeshName(std::string&& meshName);
    std::string getMeshName() const;

    void setPositions(std::vector<glm::vec3>&& positions);
    void setNormals(std::vector<glm::vec3>&& normals);
    void setTexCoords(std::vector<glm::vec2>&& texCoords);
    void setCustomAttribute(const std::string& id, VertexAttributeBuffer&& attributeBuffer);

    void setFaces(std::vector<glm::uvec3>&& faces);
    void setViews(std::vector<TriangleMeshView>&& views);

    const std::vector<glm::vec3>& getPositions() const;
    const std::vector<glm::vec3>& getNormals() const;
    const std::vector<glm::vec2>& getTexCoords() const;
    const VertexAttributeBuffer& getCustomAttribute(const std::string& id) const;

    const BoundingBox3& getBoundingBox() const;

    glm::vec3 interpolatePosition(uint32_t triangleId, const glm::vec3& barycentric) const;
    glm::vec3 interpolateNormal(uint32_t triangleId, const glm::vec3& barycentric) const;
    glm::vec2 interpolateTexCoord(uint32_t triangleId, const glm::vec3& barycentric) const;

    glm::vec3 calculateFaceNormal(uint32_t triangleId) const;
    float calculateFaceArea(uint32_t triangleId) const;

private:
    InterleavedVertexBuffer interleave(
        const std::vector<VertexAttributeDescriptor>& vertexAttribs, bool padToVec4) const;

    std::vector<glm::vec3> m_positions;
    std::vector<glm::vec3> m_normals;
    std::vector<glm::vec2> m_texCoords;
    std::vector<glm::vec4> m_tangents;
    FlatHashMap<std::string, VertexAttributeBuffer> m_customAttributes;

    std::vector<glm::uvec3> m_faces;
    std::vector<TriangleMeshView> m_views;
    std::string m_meshName;

    BoundingBox3 m_boundingBox;
};
} // namespace crisp