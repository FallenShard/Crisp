#include <Crisp/Scenes/RayTracingSceneParser.hpp>

#include <gtest/gtest.h>

namespace crisp {
namespace {

nlohmann::json createShape(nlohmann::json bsdf) {
    return {
        {"type", "mesh"},
        {"filename", "plane.obj"},
        {"bsdf", std::move(bsdf)},
    };
}

TEST(RayTracingSceneParserTest, AssignsLogicalIndicesToBitmapReflectanceTextures) {
    const nlohmann::json shapes = nlohmann::json::array({
        createShape({
            {"type", "lambertian"},
            {"reflectance", {0.2f, 0.3f, 0.4f}},
            {"reflectanceTexture", {{"type", "bitmap"}, {"filename", "Textures/uv_pattern.jpg"}}},
        }),
        createShape({
            {"type", "oren-nayar"},
            {"reflectance", {0.5f, 0.6f, 0.7f}},
            {"reflectanceTexture", {{"type", "bitmap"}, {"filename", "brick.png"}}},
        }),
    });

    const auto scene = parseSceneDescription(shapes);

    ASSERT_EQ(scene.materialTextures.size(), 2);
    EXPECT_EQ(scene.materialTextures[0].filename, "Textures/uv_pattern.jpg");
    EXPECT_EQ(scene.materialTextures[1].filename, "brick.png");
    ASSERT_EQ(scene.brdfs.size(), 2);
    EXPECT_EQ(scene.brdfs[0].reflectanceTexture, 0);
    EXPECT_EQ(scene.brdfs[1].reflectanceTexture, 1);
    EXPECT_EQ(scene.brdfs[0].reflectanceSampler, -1);
}

TEST(RayTracingSceneParserTest, RejectsInvalidBitmapReflectanceTextures) {
    const auto unsupported = nlohmann::json::array({createShape({
        {"type", "lambertian"},
        {"reflectance", {1.0f, 1.0f, 1.0f}},
        {"reflectanceTexture", {{"type", "noise"}}},
    })});
    EXPECT_THROW(parseSceneDescription(unsupported), std::invalid_argument);

    const auto missingFilename = nlohmann::json::array({createShape({
        {"type", "lambertian"},
        {"reflectance", {1.0f, 1.0f, 1.0f}},
        {"reflectanceTexture", {{"type", "bitmap"}}},
    })});
    EXPECT_THROW(parseSceneDescription(missingFilename), std::invalid_argument);

    const auto unsupportedMaterial = nlohmann::json::array({createShape({
        {"type", "rough-conductor"},
        {"reflectanceTexture", {{"type", "bitmap"}, {"filename", "brick.png"}}},
    })});
    EXPECT_THROW(parseSceneDescription(unsupportedMaterial), std::invalid_argument);
}

} // namespace
} // namespace crisp
