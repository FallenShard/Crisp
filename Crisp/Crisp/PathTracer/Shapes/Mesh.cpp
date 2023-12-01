#include <Crisp/PathTracer/Shapes/Mesh.hpp>

#include <Crisp/Math/Warp.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>

namespace crisp {
Mesh::Mesh(const VariantMap& params)
    : m_mesh(loadTriangleMesh("D:/version-control/Crisp/Resources/Meshes/" + params.get<std::string>("filename"))
                 .unwrap()) {
    m_toWorld = params.get<Transform>("toWorld");

    m_mesh.transform(m_toWorld.mat);
    m_boundingBox = m_mesh.getBoundingBox();

    m_pdf.reserve(getNumTriangles());
    for (auto i = 0; i < getNumTriangles(); i++) {
        m_pdf.append(m_mesh.calculateTriangleArea(i));
    }
    m_pdf.normalize();
}

void Mesh::fillIntersection(unsigned int triangleId, const Ray3& /*ray*/, Intersection& its) const {
    const glm::vec3 barycentric(1.0f - its.uv.x - its.uv.y, its.uv.x, its.uv.y);
    its.p = m_mesh.interpolatePosition(triangleId, barycentric);
    its.geoFrame = CoordinateFrame(m_mesh.calculateTriangleNormal(triangleId));
    its.shFrame = !m_mesh.getNormals().empty() ? CoordinateFrame(m_mesh.interpolateNormal(triangleId, barycentric))
                                               : its.geoFrame;

    if (!m_mesh.getTexCoords().empty()) {
        its.uv = m_mesh.interpolateTexCoord(triangleId, barycentric);
    }

    its.shape = this;
}

void Mesh::sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const {
    glm::vec2 sample = sampler.next2D();
    const auto triangleId = static_cast<uint32_t>(m_pdf.sampleReuse(sample.x));
    const glm::vec3 barycentric = warp::squareToUniformTriangle(sample);

    shapeSample.p = m_mesh.interpolatePosition(triangleId, barycentric);
    shapeSample.n = !m_mesh.getNormals().empty() ? m_mesh.interpolateNormal(triangleId, barycentric)
                                                 : m_mesh.calculateTriangleNormal(triangleId);
    shapeSample.pdf = m_pdf.getNormFactor();
}

float Mesh::pdfSurface(const Shape::Sample& /*shapeSample*/) const {
    return m_pdf.getNormFactor();
}

bool Mesh::addToAccelerationStructure(RTCDevice device, RTCScene scene) {
    if (m_mesh.getTriangles().empty()) {
        return false;
    }

    m_geometry = rtcNewGeometry(device, RTCGeometryType::RTC_GEOMETRY_TYPE_TRIANGLE);

    m_vertexBuffer = rtcNewBuffer(device, sizeof(glm::vec3) * m_mesh.getVertexCount());
    auto* vertices = static_cast<glm::vec3*>(rtcGetBufferData(m_vertexBuffer));
    memcpy(vertices, m_mesh.getPositions().data(), m_mesh.getVertexCount() * sizeof(glm::vec3));
    rtcSetGeometryBuffer(
        m_geometry,
        RTC_BUFFER_TYPE_VERTEX,
        0,
        RTC_FORMAT_FLOAT3,
        m_vertexBuffer,
        0,
        sizeof(glm::vec3),
        m_mesh.getVertexCount());

    m_indexBuffer = rtcNewBuffer(device, sizeof(glm::uvec3) * m_mesh.getTriangleCount());
    auto* faces = static_cast<glm::uvec3*>(rtcGetBufferData(m_indexBuffer));
    memcpy(faces, m_mesh.getTriangles().data(), m_mesh.getTriangleCount() * sizeof(glm::uvec3));
    rtcSetGeometryBuffer(
        m_geometry,
        RTC_BUFFER_TYPE_INDEX,
        0,
        RTC_FORMAT_UINT3,
        m_indexBuffer,
        0,
        sizeof(glm::uvec3),
        m_mesh.getTriangleCount());

    rtcCommitGeometry(m_geometry);
    m_geometryId = rtcAttachGeometry(scene, m_geometry);
    return true;
}

size_t Mesh::getNumTriangles() const {
    return m_mesh.getTriangleCount();
}

size_t Mesh::getNumVertices() const {
    return m_mesh.getVertexCount();
}

const std::vector<glm::vec3>& Mesh::getVertexPositions() const {
    return m_mesh.getPositions();
}

const std::vector<glm::uvec3>& Mesh::getTriangleIndices() const {
    return m_mesh.getTriangles();
}
} // namespace crisp