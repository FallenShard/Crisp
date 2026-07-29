#include <gtest/gtest.h>

#include <Crisp/Mesh/Io/ExternalAssetConfig.hpp>
#include <Crisp/Mesh/Io/WavefrontObjLoader.hpp>

namespace crisp {
namespace {

struct MeshExpectation {
    const char* filename;
    std::size_t positions;
    std::size_t triangles;
};

void PrintTo(const MeshExpectation& expectation, std::ostream* stream) {
    *stream << expectation.filename;
}

class WavefrontObjExternalAssetTest : public testing::TestWithParam<MeshExpectation> {};

TEST_P(WavefrontObjExternalAssetTest, LoadsLargeRegressionMesh) {
    if (test::kExternalAssetDir.empty()) {
        GTEST_SKIP() << "Set CRISP_EXTERNAL_ASSET_DIR to the full Crisp Resources directory";
    }

    const auto expectation = GetParam();
    const auto mesh = loadWavefrontObj(test::kExternalAssetDir / "Meshes" / expectation.filename);
    EXPECT_EQ(mesh.positions.size(), expectation.positions);
    EXPECT_EQ(mesh.triangles.size(), expectation.triangles);
}

INSTANTIATE_TEST_SUITE_P(
    FullAssetPack,
    WavefrontObjExternalAssetTest,
    testing::Values(
        MeshExpectation{"ajax.obj", 409'676, 544'566},
        MeshExpectation{"buddha.obj", 49'990, 100'000},
        MeshExpectation{"shader_ball.obj", 35'877, 67'832},
        MeshExpectation{"camelhead.obj", 11'381, 22'704}),
    [](const testing::TestParamInfo<MeshExpectation>& info) {
        return std::filesystem::path{info.param.filename}.stem().string();
    });

} // namespace
} // namespace crisp
