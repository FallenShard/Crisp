#include <gmock/gmock.h>

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
