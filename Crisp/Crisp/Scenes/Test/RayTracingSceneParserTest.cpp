#include <Crisp/Scenes/RayTracingSceneParser.hpp>

#include <gtest/gtest.h>

#include <Crisp/Core/Test/ResultTestUtils.hpp>

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

    const auto result = parseSceneDescription(shapes);
    ASSERT_TRUE(result.hasValue());
    const auto& scene = *result;

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
    EXPECT_THAT(parseSceneDescription(unsupported), HasErrorWithMessageRegex("reflectance texture type"));

    const auto missingFilename = nlohmann::json::array({createShape({
        {"type", "lambertian"},
        {"reflectance", {1.0f, 1.0f, 1.0f}},
        {"reflectanceTexture", {{"type", "bitmap"}}},
    })});
    EXPECT_THAT(parseSceneDescription(missingFilename), HasErrorWithMessageRegex("requires a filename"));

    const auto unsupportedMaterial = nlohmann::json::array({createShape({
        {"type", "rough-conductor"},
        {"reflectanceTexture", {{"type", "bitmap"}, {"filename", "brick.png"}}},
    })});
    EXPECT_THAT(parseSceneDescription(unsupportedMaterial), HasErrorWithMessageRegex("non-diffuse"));
}

TEST(RayTracingSceneParserTest, RejectsUnsupportedBsdfInsteadOfUsingFallbackMaterial) {
    const auto shapes = nlohmann::json::array({createShape({{"type", "not-a-bsdf"}})});
    EXPECT_THAT(parseSceneDescription(shapes), HasErrorWithMessageRegex("Unsupported GPU BSDF type"));
}

TEST(RayTracingSceneParserTest, PropagatesNestedMaterialValidationErrors) {
    const auto invalidDistribution = nlohmann::json::array({createShape({
        {"type", "rough-conductor"},
        {"microfacetDistribution", "phong"},
    })});
    EXPECT_THAT(
        parseSceneDescription(invalidDistribution),
        HasErrorWithMessageRegex("Unsupported microfacet distribution"));

    const auto invalidIors = nlohmann::json::array({createShape({
        {"type", "dielectric"},
        {"interiorIor", 1.0f},
        {"exteriorIor", 1.0f},
    })});
    EXPECT_THAT(parseSceneDescription(invalidIors), HasErrorWithMessageRegex("IORs must be finite"));

    const auto invalidReflectance = nlohmann::json::array({createShape({
        {"type", "lambertian"},
        {"reflectance", {1.0f, 1.0f}},
    })});
    EXPECT_THAT(parseSceneDescription(invalidReflectance), HasErrorWithMessageRegex("exactly three numbers"));
}

TEST(RayTracingSceneParserTest, ParsesStandalonePointLights) {
    const nlohmann::json lights = nlohmann::json::array({{
        {"type", "point"},
        {"position", {1.0f, 2.0f, 3.0f}},
        {"power", {10.0f, 20.0f, 30.0f}},
    }});

    const auto result = parseSceneDescription(nlohmann::json::array(), lights);
    ASSERT_TRUE(result.hasValue());
    ASSERT_EQ(result->lights.size(), 1);
    EXPECT_EQ(result->lights[0].type, kLightPoint);
    EXPECT_EQ(result->lights[0].meshId, -1);
    EXPECT_EQ(result->lights[0].positionOrDirection, glm::vec3(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(result->lights[0].emission, glm::vec3(10.0f, 20.0f, 30.0f));
}

TEST(RayTracingSceneParserTest, RejectsInvalidPointLights) {
    const nlohmann::json missingPosition = nlohmann::json::array({{
        {"type", "point"},
        {"power", {10.0f, 20.0f, 30.0f}},
    }});
    EXPECT_THAT(
        parseSceneDescription(nlohmann::json::array(), missingPosition),
        HasErrorWithMessageRegex("requires a position"));

    const nlohmann::json missingPower = nlohmann::json::array({{
        {"type", "point"},
        {"position", {1.0f, 2.0f, 3.0f}},
    }});
    EXPECT_THAT(
        parseSceneDescription(nlohmann::json::array(), missingPower),
        HasErrorWithMessageRegex("requires power"));

    const nlohmann::json negativePower = nlohmann::json::array({{
        {"type", "point"},
        {"position", {1.0f, 2.0f, 3.0f}},
        {"power", {10.0f, -1.0f, 30.0f}},
    }});
    EXPECT_THAT(
        parseSceneDescription(nlohmann::json::array(), negativePower),
        HasErrorWithMessageRegex("power .* must be non-negative"));
}

TEST(RayTracingSceneParserTest, ParsesStandaloneDirectionalLights) {
    const nlohmann::json lights = nlohmann::json::array({{
        {"type", "directional"},
        {"direction", {0.0f, -3.0f, 4.0f}},
        {"power", {1.0f, 2.0f, 3.0f}},
    }});

    const auto result = parseSceneDescription(nlohmann::json::array(), lights);
    ASSERT_TRUE(result.hasValue());
    ASSERT_EQ(result->lights.size(), 1);
    EXPECT_EQ(result->lights[0].type, kLightDirectional);
    EXPECT_EQ(result->lights[0].meshId, -1);
    EXPECT_EQ(result->lights[0].positionOrDirection, glm::vec3(0.0f, -0.6f, 0.8f));
    EXPECT_EQ(result->lights[0].emission, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(RayTracingSceneParserTest, RejectsInvalidDirectionalLights) {
    const nlohmann::json missingDirection = nlohmann::json::array({{
        {"type", "directional"},
        {"power", {1.0f, 2.0f, 3.0f}},
    }});
    EXPECT_THAT(
        parseSceneDescription(nlohmann::json::array(), missingDirection),
        HasErrorWithMessageRegex("requires a direction"));

    const nlohmann::json missingPower = nlohmann::json::array({{
        {"type", "directional"},
        {"direction", {0.0f, 0.0f, -1.0f}},
    }});
    EXPECT_THAT(
        parseSceneDescription(nlohmann::json::array(), missingPower),
        HasErrorWithMessageRegex("requires power"));

    const nlohmann::json zeroDirection = nlohmann::json::array({{
        {"type", "directional"},
        {"direction", {0.0f, 0.0f, 0.0f}},
        {"power", {1.0f, 2.0f, 3.0f}},
    }});
    EXPECT_THAT(
        parseSceneDescription(nlohmann::json::array(), zeroDirection),
        HasErrorWithMessageRegex("finite, non-zero length"));

    const nlohmann::json negativePower = nlohmann::json::array({{
        {"type", "directional"},
        {"direction", {0.0f, 0.0f, -1.0f}},
        {"power", {1.0f, -1.0f, 3.0f}},
    }});
    EXPECT_THAT(
        parseSceneDescription(nlohmann::json::array(), negativePower),
        HasErrorWithMessageRegex("power .* must be non-negative"));
}

} // namespace
} // namespace crisp
