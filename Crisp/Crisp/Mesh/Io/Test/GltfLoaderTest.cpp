#include <gmock/gmock.h>

#include <cstring>

#include <Crisp/Mesh/Io/ExternalAssetConfig.hpp>
#include <Crisp/Mesh/Io/GltfLoader.hpp>

namespace crisp {
namespace {

using ::testing::SizeIs;

MATCHER(HasValue, "") {
    *result_listener << "where the Result type has an error!";
    return arg.hasValue();
}

TEST(GltfLoaderTest, LoadsTrackedTriangle) {
    auto asset = loadGltfAsset(std::filesystem::path{"TestData"} / "CrispGltfLoaderTest" / "Triangle.gltf");
    ASSERT_THAT(asset, HasValue());

    const auto loaded = asset.unwrap();
    ASSERT_THAT(loaded.models, SizeIs(1));
    EXPECT_EQ(loaded.models[0].mesh.getVertexCount(), 3);
    EXPECT_EQ(loaded.models[0].mesh.getTriangleCount(), 1);
    EXPECT_EQ(loaded.models[0].mesh.getTriangles()[0], glm::uvec3(0, 1, 2));
    EXPECT_FALSE(loaded.models[0].mesh.hasCustomAttribute("weights0"));
    EXPECT_FALSE(loaded.models[0].mesh.hasCustomAttribute("indices0"));
}

TEST(GltfLoaderTest, LoadsNonIndexedTriangle) {
    auto asset = loadGltfAsset(std::filesystem::path{"TestData"} / "CrispGltfLoaderTest" / "NonIndexedTriangle.gltf");
    ASSERT_THAT(asset, HasValue());

    const auto loaded = asset.unwrap();
    ASSERT_THAT(loaded.models, SizeIs(1));
    ASSERT_THAT(loaded.models[0].mesh.getTriangles(), SizeIs(1));
    EXPECT_EQ(loaded.models[0].mesh.getTriangles()[0], glm::uvec3(0, 1, 2));
    ASSERT_THAT(loaded.models[0].mesh.getTangents(), SizeIs(3));
    EXPECT_EQ(loaded.models[0].mesh.getTangents()[0], glm::vec4(1.0f, 0.0f, 0.0f, -1.0f));
}

TEST(GltfLoaderTest, LoadsUnsignedByteIndices) {
    auto asset =
        loadGltfAsset(std::filesystem::path{"TestData"} / "CrispGltfLoaderTest" / "UnsignedByteIndicesTriangle.gltf");
    ASSERT_THAT(asset, HasValue());

    const auto loaded = asset.unwrap();
    ASSERT_THAT(loaded.models, SizeIs(1));
    ASSERT_THAT(loaded.models[0].mesh.getTriangles(), SizeIs(1));
    EXPECT_EQ(loaded.models[0].mesh.getTriangles()[0], glm::uvec3(0, 1, 2));
}

TEST(GltfLoaderTest, AccumulatesParentNodeTransforms) {
    auto asset =
        loadGltfAsset(std::filesystem::path{"TestData"} / "CrispGltfLoaderTest" / "ParentTransformTriangle.gltf");
    ASSERT_THAT(asset, HasValue());

    const auto loaded = asset.unwrap();
    ASSERT_THAT(loaded.models, SizeIs(1));
    const glm::vec3 worldOrigin = glm::vec3(loaded.models[0].transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_FLOAT_EQ(worldOrigin.x, 1.0f);
    EXPECT_FLOAT_EQ(worldOrigin.y, 6.0f);
    EXPECT_FLOAT_EQ(worldOrigin.z, 0.0f);
}

TEST(GltfLoaderTest, PreservesSkinningAcrossMultiplePrimitives) {
    auto asset =
        loadGltfAsset(std::filesystem::path{"TestData"} / "CrispGltfLoaderTest" / "MultiPrimitiveSkinnedTriangle.gltf");
    ASSERT_THAT(asset, HasValue());

    const auto loaded = asset.unwrap();
    ASSERT_THAT(loaded.models, SizeIs(2));
    for (const auto& model : loaded.models) {
        EXPECT_THAT(model.skinningData.skeleton.joints, SizeIs(1));
        EXPECT_THAT(model.skinningData.inverseBindTransforms, SizeIs(1));
        ASSERT_THAT(model.skinningData.skeleton.parents, SizeIs(1));
        EXPECT_EQ(model.skinningData.skeleton.parents[0], -1);
        EXPECT_EQ(model.skinningData.inverseBindTransforms[0], glm::mat4(1.0f));
        ASSERT_TRUE(model.skinningData.modelNodeToLinearIdx.contains(1));
        EXPECT_EQ(model.skinningData.modelNodeToLinearIdx.at(1), 0);
        EXPECT_TRUE(model.mesh.hasCustomAttribute("weights0"));
        EXPECT_TRUE(model.mesh.hasCustomAttribute("indices0"));
        EXPECT_EQ(model.mesh.getCustomAttribute("weights0").buffer.size(), 3 * sizeof(glm::vec4));
        EXPECT_EQ(model.mesh.getCustomAttribute("indices0").buffer.size(), 3 * sizeof(glm::uvec4));
        glm::vec4 firstWeights{};
        std::memcpy(&firstWeights, model.mesh.getCustomAttribute("weights0").buffer.data(), sizeof(firstWeights));
        EXPECT_EQ(firstWeights, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
        ASSERT_THAT(model.animations, SizeIs(1));
        ASSERT_THAT(model.animations[0].channels, SizeIs(1));
        EXPECT_EQ(model.animations[0].channels[0].targetNode, 0);
    }
}

TEST(GltfLoaderTest, LoadsMeshlessScene) {
    auto asset = loadGltfAsset(std::filesystem::path{"TestData"} / "CrispGltfLoaderTest" / "MeshlessScene.gltf");
    ASSERT_THAT(asset, HasValue());

    const auto loaded = asset.unwrap();
    EXPECT_TRUE(loaded.models.empty());
    EXPECT_TRUE(loaded.images.albedoMaps.empty());
    EXPECT_TRUE(loaded.images.normalMaps.empty());
    EXPECT_TRUE(loaded.images.ormMaps.empty());
    EXPECT_TRUE(loaded.images.emissiveMaps.empty());
}

TEST(GltfLoaderTest, LoadsEmbeddedImagesInSourceOrder) {
    auto asset = loadGltfAsset(std::filesystem::path{"TestData"} / "CrispGltfLoaderTest" / "TexturedTriangle.gltf");
    ASSERT_THAT(asset, HasValue());

    const auto loaded = asset.unwrap();
    ASSERT_THAT(loaded.models, SizeIs(1));
    ASSERT_THAT(loaded.images.albedoMaps, SizeIs(1));
    ASSERT_THAT(loaded.images.normalMaps, SizeIs(1));
    ASSERT_THAT(loaded.images.ormMaps, SizeIs(1));
    EXPECT_EQ(loaded.images.ormMaps[0].getChannelCount(), 4);
    EXPECT_EQ(loaded.models[0].material.textureKeys[0], "TexturedTriangle-albedo-0");
    EXPECT_EQ(loaded.models[0].material.textureKeys[1], "TexturedTriangle-normal-0");
    EXPECT_EQ(loaded.models[0].material.textureKeys[2], "TexturedTriangle-orm-0");
    EXPECT_EQ(loaded.models[0].material.name, "TestMaterial");
    EXPECT_EQ(loaded.models[0].material.params.baseColor, glm::vec3(0.2f, 0.4f, 0.6f));
    EXPECT_FLOAT_EQ(loaded.models[0].material.params.geometryOpacity, 0.8f);
    EXPECT_FLOAT_EQ(loaded.models[0].material.params.baseMetalness, 0.35f);
    EXPECT_FLOAT_EQ(loaded.models[0].material.params.specularRoughness, 0.65f);
    EXPECT_FLOAT_EQ(loaded.models[0].material.params.emissionColor.r, 1.0f / 3.0f);
    EXPECT_FLOAT_EQ(loaded.models[0].material.params.emissionColor.g, 2.0f / 3.0f);
    EXPECT_FLOAT_EQ(loaded.models[0].material.params.emissionColor.b, 1.0f);
    EXPECT_FLOAT_EQ(loaded.models[0].material.params.emissionLuminance, 0.75f);
    EXPECT_FLOAT_EQ(loaded.models[0].material.params.normalScale, 0.25f);
    EXPECT_FLOAT_EQ(loaded.models[0].material.params.aoStrength, 0.5f);
}

TEST(GltfLoaderTest, DeduplicatesEmbeddedImagesByContent) {
    auto asset =
        loadGltfAsset(std::filesystem::path{"TestData"} / "CrispGltfLoaderTest" / "DuplicateTexturedTriangle.gltf");
    ASSERT_THAT(asset, HasValue());

    const auto loaded = asset.unwrap();
    ASSERT_THAT(loaded.models, SizeIs(2));
    ASSERT_THAT(loaded.images.albedoMaps, SizeIs(1));
    EXPECT_EQ(loaded.models[0].material.textureKeys[0], "DuplicateTexturedTriangle-albedo-0");
    EXPECT_EQ(loaded.models[1].material.textureKeys[0], "DuplicateTexturedTriangle-albedo-0");
}

TEST(GltfLoaderTest, LoadsUsdShaderBallForMaterialExplorer) {
    const auto path =
        std::filesystem::path{CRISP_RESOURCE_DIR} / "glTFSamples" / "2.0" / "USDShaderBallForGltf" / "glTF-Binary" /
        "USDShaderBallForGltf.glb";
    auto asset = loadGltfAsset(path);
    ASSERT_THAT(asset, HasValue());

    const auto loaded = asset.unwrap();
    ASSERT_THAT(loaded.models, SizeIs(4));
    EXPECT_EQ(loaded.models[0].material.name, "core");
    EXPECT_EQ(loaded.models[1].material.name, "core");
    EXPECT_EQ(loaded.models[2].material.name, "material_surface");
    EXPECT_EQ(loaded.models[3].material.name, "sss_bars");
    EXPECT_THAT(loaded.images.albedoMaps, SizeIs(1));
    EXPECT_THAT(loaded.images.ormMaps, SizeIs(2));
}

TEST(GltfLoaderTest, LoadsAvocadoFromExternalAssetPack) {
    if (test::kExternalAssetDir.empty()) {
        GTEST_SKIP() << "Set CRISP_EXTERNAL_ASSET_DIR to the full Crisp Resources directory";
    }

    auto asset = loadGltfAsset(test::kExternalAssetDir / "glTFSamples" / "2.0" / "Avocado" / "glTF" / "Avocado.gltf");
    ASSERT_THAT(asset, HasValue());
    EXPECT_EQ(asset.unwrap().models[0].mesh.getVertexCount(), 406);
}

} // namespace
} // namespace crisp
