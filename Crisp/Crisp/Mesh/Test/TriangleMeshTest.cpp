#include <gmock/gmock.h>

#include <cstring>

#include <Crisp/Mesh/TriangleMesh.hpp>

namespace crisp {
namespace {

using ::testing::ElementsAre;
using ::testing::SizeIs;

TriangleMesh createTriangle(const float offset) {
    return {
        {{offset, 0.0f, 0.0f}, {offset + 1.0f, 0.0f, 0.0f}, {offset, 1.0f, 0.0f}},
        {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}},
        {{0, 1, 2}}
    };
}

std::vector<uint32_t> readAttribute(const TriangleMesh& mesh, const std::string_view name) {
    const auto& attribute = mesh.getCustomAttribute(name);
    std::vector<uint32_t> values(attribute.buffer.size() / sizeof(uint32_t));
    std::memcpy(values.data(), attribute.buffer.data(), attribute.buffer.size());
    return values;
}

void setAttribute(TriangleMesh& mesh, const std::string_view name, const std::vector<uint32_t>& values) {
    mesh.setCustomAttribute(name, createCustomVertexAttributeBuffer<uint32_t>(values));
}

TEST(TriangleMeshTest, AppendMergesCustomAttributesPresentOnBothSides) {
    TriangleMesh mesh{createTriangle(0.0f)};
    setAttribute(mesh, "id", {1, 2, 3});

    TriangleMesh other{createTriangle(2.0f)};
    setAttribute(other, "id", {4, 5, 6});

    mesh.append(std::move(other));

    EXPECT_EQ(mesh.getVertexCount(), 6);
    EXPECT_THAT(readAttribute(mesh, "id"), ElementsAre(1, 2, 3, 4, 5, 6));
}

TEST(TriangleMeshTest, AppendPadsAnAttributeTheAppendedMeshLacks) {
    TriangleMesh mesh{createTriangle(0.0f)};
    setAttribute(mesh, "id", {1, 2, 3});

    mesh.append(createTriangle(2.0f));

    EXPECT_EQ(mesh.getVertexCount(), 6);
    EXPECT_THAT(readAttribute(mesh, "id"), ElementsAre(1, 2, 3, 0, 0, 0));
}

TEST(TriangleMeshTest, AppendPadsAnAttributeOnlyTheAppendedMeshCarries) {
    TriangleMesh mesh{createTriangle(0.0f)};

    TriangleMesh other{createTriangle(2.0f)};
    setAttribute(other, "id", {4, 5, 6});

    mesh.append(std::move(other));

    EXPECT_EQ(mesh.getVertexCount(), 6);
    EXPECT_TRUE(mesh.hasCustomAttribute("id"));
    EXPECT_THAT(readAttribute(mesh, "id"), ElementsAre(0, 0, 0, 4, 5, 6));
}

TEST(TriangleMeshTest, AppendKeepsAttributesIndexedByTheRebasedTriangles) {
    TriangleMesh mesh{createTriangle(0.0f)};
    setAttribute(mesh, "id", {1, 2, 3});

    TriangleMesh other{createTriangle(2.0f)};
    setAttribute(other, "id", {4, 5, 6});

    mesh.append(std::move(other));

    ASSERT_THAT(mesh.getTriangles(), SizeIs(2));
    EXPECT_EQ(mesh.getTriangles()[1], glm::uvec3(3, 4, 5));

    const auto ids = readAttribute(mesh, "id");
    EXPECT_EQ(ids[mesh.getTriangles()[1][0]], 4);
}

} // namespace
} // namespace crisp
