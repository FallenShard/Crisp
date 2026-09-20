#include <Crisp/Mesh/MeshOptimizer.hpp>

#include <meshoptimizer.h>

#include <Crisp/Core/Checks.hpp>

namespace crisp {
namespace {

constexpr uint32_t kAnalysisCacheSize = 16;
constexpr uint32_t kAnalysisWarpSize = 0;
constexpr uint32_t kAnalysisPrimitiveGroupSize = 0;

constexpr float kOverdrawThreshold = 1.05f;

struct IndexRange {
    uint32_t firstIndex;
    uint32_t indexCount;
};

// A view's index range is what a draw call submits, so each is optimized on its own; reordering triangles across
// range boundaries would move them between draws. A mesh without views is one range covering every triangle.
std::vector<IndexRange> collectIndexRanges(const TriangleMesh& mesh) {
    const uint32_t indexCount = mesh.getIndexCount();
    if (mesh.getViews().empty()) {
        return {IndexRange{.firstIndex = 0, .indexCount = indexCount}};
    }

    std::vector<IndexRange> ranges;
    ranges.reserve(mesh.getViews().size());
    for (const auto& view : mesh.getViews()) {
        CRISP_CHECK_LE(view.firstIndex + view.indexCount, indexCount, "Mesh view range exceeds the index buffer.");
        CRISP_CHECK_EQ(view.firstIndex % 3, 0, "Mesh view range must start on a triangle boundary.");
        CRISP_CHECK_EQ(view.indexCount % 3, 0, "Mesh view range must hold whole triangles.");
        if (view.indexCount > 0) {
            ranges.push_back(IndexRange{.firstIndex = view.firstIndex, .indexCount = view.indexCount});
        }
    }
    return ranges;
}

std::vector<meshopt_Stream> collectVertexStreams(const TriangleMesh& mesh, const std::vector<std::string>& customNames) {
    std::vector<meshopt_Stream> streams;
    const auto addStream = [&streams](const auto& data, const size_t elementSize) {
        streams.push_back(meshopt_Stream{.data = data, .size = elementSize, .stride = elementSize});
    };

    if (mesh.hasPositions()) {
        addStream(mesh.getPositions().data(), sizeof(glm::vec3));
    }
    if (mesh.hasNormals()) {
        addStream(mesh.getNormals().data(), sizeof(glm::vec3));
    }
    if (mesh.hasTexCoords()) {
        addStream(mesh.getTexCoords().data(), sizeof(glm::vec2));
    }
    if (mesh.hasTangents()) {
        addStream(mesh.getTangents().data(), sizeof(glm::vec4));
    }
    for (const auto& name : customNames) {
        const auto& attribute = mesh.getCustomAttribute(name);
        addStream(attribute.buffer.data(), attribute.descriptor.size);
    }
    return streams;
}

// The byte footprint of one vertex across every stream, which is what makes the overfetch figure meaningful.
size_t computeVertexSize(const TriangleMesh& mesh, const std::vector<std::string>& customNames) {
    size_t vertexSize = 0;
    if (mesh.hasPositions()) {
        vertexSize += sizeof(glm::vec3);
    }
    if (mesh.hasNormals()) {
        vertexSize += sizeof(glm::vec3);
    }
    if (mesh.hasTexCoords()) {
        vertexSize += sizeof(glm::vec2);
    }
    if (mesh.hasTangents()) {
        vertexSize += sizeof(glm::vec4);
    }
    for (const auto& name : customNames) {
        vertexSize += mesh.getCustomAttribute(name).descriptor.size;
    }
    return vertexSize;
}

template <typename T>
void remapTypedStream(
    const std::vector<T>& source,
    const size_t uniqueVertexCount,
    const std::vector<uint32_t>& remap,
    std::vector<T>& destination) {
    if (source.empty()) {
        destination.clear();
        return;
    }

    destination.assign(uniqueVertexCount, T{});
    meshopt_remapVertexBuffer(destination.data(), source.data(), source.size(), sizeof(T), remap.data());
}

// Applies one vertex remap to every stream the mesh carries, so that all attributes stay addressed by the same
// index buffer afterwards.
void remapAllVertexStreams(
    TriangleMesh& mesh,
    const std::vector<std::string>& customNames,
    const size_t uniqueVertexCount,
    const std::vector<uint32_t>& remap) {
    if (mesh.hasPositions()) {
        std::vector<glm::vec3> remapped;
        remapTypedStream(mesh.getPositions(), uniqueVertexCount, remap, remapped);
        mesh.setPositions(std::move(remapped));
    }
    if (mesh.hasNormals()) {
        std::vector<glm::vec3> remapped;
        remapTypedStream(mesh.getNormals(), uniqueVertexCount, remap, remapped);
        mesh.setNormals(std::move(remapped));
    }
    if (mesh.hasTexCoords()) {
        std::vector<glm::vec2> remapped;
        remapTypedStream(mesh.getTexCoords(), uniqueVertexCount, remap, remapped);
        mesh.setTexCoords(std::move(remapped));
    }
    if (mesh.hasTangents()) {
        std::vector<glm::vec4> remapped;
        remapTypedStream(mesh.getTangents(), uniqueVertexCount, remap, remapped);
        mesh.setTangents(std::move(remapped));
    }

    for (const auto& name : customNames) {
        const auto& attribute = mesh.getCustomAttribute(name);
        const size_t elementSize = attribute.descriptor.size;
        const size_t sourceCount = attribute.buffer.size() / elementSize;

        VertexAttributeBuffer remapped{};
        remapped.descriptor = attribute.descriptor;
        remapped.buffer.assign(uniqueVertexCount * elementSize, std::byte{});
        meshopt_remapVertexBuffer(
            remapped.buffer.data(), attribute.buffer.data(), sourceCount, elementSize, remap.data());
        mesh.setCustomAttribute(name, std::move(remapped));
    }
}

meshopt_VertexCacheStatistics analyzeCache(const std::vector<glm::uvec3>& triangles, const size_t vertexCount) {
    return meshopt_analyzeVertexCache(
        glm::value_ptr(triangles[0]),
        triangles.size() * 3,
        vertexCount,
        kAnalysisCacheSize,
        kAnalysisWarpSize,
        kAnalysisPrimitiveGroupSize);
}

} // namespace

void MeshOptimizationStats::accumulate(const MeshOptimizationStats& other) {
    // The ratios are per-mesh, so a scene total weights them by index count rather than treating a two-triangle
    // mesh as the equal of a thirteen-thousand-triangle one.
    const uint32_t totalIndexCount = indexCount + other.indexCount;
    if (totalIndexCount > 0) {
        const float selfWeight = static_cast<float>(indexCount) / static_cast<float>(totalIndexCount);
        const float otherWeight = static_cast<float>(other.indexCount) / static_cast<float>(totalIndexCount);

        acmrBefore = acmrBefore * selfWeight + other.acmrBefore * otherWeight;
        acmrAfter = acmrAfter * selfWeight + other.acmrAfter * otherWeight;
        atvrBefore = atvrBefore * selfWeight + other.atvrBefore * otherWeight;
        atvrAfter = atvrAfter * selfWeight + other.atvrAfter * otherWeight;
        overfetchBefore = overfetchBefore * selfWeight + other.overfetchBefore * otherWeight;
        overfetchAfter = overfetchAfter * selfWeight + other.overfetchAfter * otherWeight;
    }

    vertexCountBefore += other.vertexCountBefore;
    vertexCountAfter += other.vertexCountAfter;
    indexCount = totalIndexCount;
}

MeshOptimizationStats optimizeMeshIndices(TriangleMesh& mesh) {
    MeshOptimizationStats stats{};
    if (!mesh.hasTriangles() || !mesh.hasPositions()) {
        return stats;
    }

    std::vector<std::string> customNames;
    customNames.reserve(mesh.getCustomAttributes().size());
    for (const auto& [name, attribute] : mesh.getCustomAttributes()) {
        CRISP_CHECK_GT(attribute.descriptor.size, 0, "A custom vertex attribute must declare its element size.");
        customNames.push_back(name);
    }

    std::vector<glm::uvec3> triangles{mesh.getTriangles()};
    const size_t indexCount = triangles.size() * 3;
    const size_t vertexSize = computeVertexSize(mesh, customNames);

    stats.indexCount = static_cast<uint32_t>(indexCount);
    stats.vertexCountBefore = mesh.getVertexCount();
    const auto cacheBefore = analyzeCache(triangles, stats.vertexCountBefore);
    stats.acmrBefore = cacheBefore.acmr;
    stats.atvrBefore = cacheBefore.atvr;
    stats.overfetchBefore =
        meshopt_analyzeVertexFetch(glm::value_ptr(triangles[0]), indexCount, stats.vertexCountBefore, vertexSize)
            .overfetch;

    {
        const auto streams = collectVertexStreams(mesh, customNames);
        std::vector<uint32_t> remap(mesh.getVertexCount());
        const size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(
            remap.data(), glm::value_ptr(triangles[0]), indexCount, mesh.getVertexCount(), streams.data(), streams.size());

        std::vector<glm::uvec3> welded(triangles.size());
        meshopt_remapIndexBuffer(glm::value_ptr(welded[0]), glm::value_ptr(triangles[0]), indexCount, remap.data());
        triangles = std::move(welded);

        remapAllVertexStreams(mesh, customNames, uniqueVertexCount, remap);
    }

    const auto ranges = collectIndexRanges(mesh);
    const uint32_t weldedVertexCount = mesh.getVertexCount();

    std::vector<glm::uvec3> scratch{triangles};
    for (const auto& range : ranges) {
        const size_t triangleOffset = range.firstIndex / 3;
        meshopt_optimizeVertexCache(
            glm::value_ptr(scratch[triangleOffset]),
            glm::value_ptr(triangles[triangleOffset]),
            range.indexCount,
            weldedVertexCount);
    }
    std::swap(triangles, scratch);

    for (const auto& range : ranges) {
        const size_t triangleOffset = range.firstIndex / 3;
        meshopt_optimizeOverdraw(
            glm::value_ptr(scratch[triangleOffset]),
            glm::value_ptr(triangles[triangleOffset]),
            range.indexCount,
            mesh.getPositionsPtr(),
            weldedVertexCount,
            sizeof(glm::vec3),
            kOverdrawThreshold);
    }
    std::swap(triangles, scratch);

    {
        std::vector<uint32_t> fetchRemap(weldedVertexCount);
        const size_t finalVertexCount = meshopt_optimizeVertexFetchRemap(
            fetchRemap.data(), glm::value_ptr(triangles[0]), indexCount, weldedVertexCount);

        meshopt_remapIndexBuffer(
            glm::value_ptr(scratch[0]), glm::value_ptr(triangles[0]), indexCount, fetchRemap.data());
        std::swap(triangles, scratch);

        remapAllVertexStreams(mesh, customNames, finalVertexCount, fetchRemap);
    }

    mesh.setTriangles(std::move(triangles));

    stats.vertexCountAfter = mesh.getVertexCount();
    const auto cacheAfter = analyzeCache(mesh.getTriangles(), stats.vertexCountAfter);
    stats.acmrAfter = cacheAfter.acmr;
    stats.atvrAfter = cacheAfter.atvr;
    stats.overfetchAfter =
        meshopt_analyzeVertexFetch(mesh.getIndices(), indexCount, stats.vertexCountAfter, vertexSize).overfetch;
    return stats;
}

} // namespace crisp
