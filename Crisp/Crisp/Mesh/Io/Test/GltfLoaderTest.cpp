#include <gmock/gmock.h>

#include <Crisp/Mesh/Io/GltfLoader.hpp>

namespace crisp {
namespace {
const std::filesystem::path ResourceDir("D:/Projects/Crisp/Resources/glTFSamples/2.0");

std::filesystem::path getGltfPath(const std::string& gltfName) {
    return ResourceDir / gltfName / "glTF" / (gltfName + ".gltf");
}

MATCHER(HasValue, "") {
    *result_listener << "where the Result type has an error!";
    return arg.hasValue();
}

MATCHER(HasError, "") {
    *result_listener << "where the Result type has a value!";
    return !arg.hasValue();
}

TEST(GltfLoaderTest, LoadAvocado) {
    auto asset = loadGltfAsset(getGltfPath("Avocado"));
    EXPECT_THAT(asset, HasValue());
    const auto mesh = asset.unwrap();
    EXPECT_EQ(mesh.models[0].mesh.getVertexCount(), 406);
}

} // namespace
} // namespace crisp
