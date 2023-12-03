#include <Crisp/Scenes/RayTracingSceneParser.hpp>

namespace crisp {
namespace {
constexpr int32_t kBrdfLambertian = 0;
constexpr int32_t kBrdfDielectric = 1;
constexpr int32_t kBrdfMirror = 2;
constexpr int32_t kBrdfMicrofacet = 3;

constexpr int32_t kLightArea = 0;

BrdfParameters createLambertianBrdf(glm::vec3 albedo) {
    return {
        .albedo = albedo,
        .type = kBrdfLambertian,
    };
}

BrdfParameters createDielectricBrdf(const float intIor) {
    return {
        .type = kBrdfDielectric,
        .intIor = intIor,
    };
}

BrdfParameters createMirrorBrdf() {
    return {
        .type = kBrdfMirror,
    };
}

BrdfParameters parseBrdfParameters(const nlohmann::json& brdf) {
    const auto& type{brdf["type"]};
    if (type == "lambertian") {
        return createLambertianBrdf(parseVec3(brdf["albedo"]));
    }
    if (type == "dielectric") {
        return createDielectricBrdf(brdf["intIOR"].get<float>());
    }
    if (type == "mirror") {
        return createMirrorBrdf();
    }
    if (type == "microfacet") {
        return createMicrofacetBrdf(glm::vec3(0.5, 0.2, 0.0), 0.2f);
    }
    return createLambertianBrdf(glm::vec3(1.0, 1.0, 0.0));
}

} // namespace

BrdfParameters createMicrofacetBrdf(const glm::vec3 kd, const float alpha) {
    return {
        .type = kBrdfMicrofacet,
        .intIor = Fresnel::getIOR(IndexOfRefraction::Glass),
        .kd = kd,
        .ks = 1.0f - std::max(kd.x, std::max(kd.y, kd.z)),
        .microfacetAlpha = alpha,
    };
}

glm::vec3 parseVec3(const nlohmann::json& json) {
    return {json[0].get<float>(), json[1].get<float>(), json[2].get<float>()};
}

SceneDescription parseSceneDescription(const nlohmann::json& shapeList) {
    SceneDescription scene{};
    for (const auto& shape : shapeList) {
        scene.meshFilenames.push_back(shape["path"]);

        scene.brdfs.push_back(parseBrdfParameters(shape["brdf"]));

        if (shape.contains("light")) {
            const auto lightIdx = static_cast<int32_t>(scene.lights.size());
            scene.props.push_back({.materialId = static_cast<int32_t>(scene.brdfs.size() - 1), .lightId = lightIdx});

            const auto meshIdx = static_cast<int32_t>(scene.meshFilenames.size() - 1);
            scene.lights.push_back({
                .type = kLightArea,
                .meshId = meshIdx,
                .radiance = parseVec3(shape["light"]["radiance"]),
            });
        } else {
            scene.props.push_back({.materialId = static_cast<int32_t>(scene.brdfs.size() - 1), .lightId = -1});
        }

        const glm::mat4 translation =
            shape.contains("translation") ? glm::translate(parseVec3(shape["translation"])) : glm::mat4(1.0f);
        const glm::mat4 rotation = glm::mat4(1.0f);
        const glm::mat4 scale =
            shape.contains("scale") ? glm::scale(glm::vec3(shape["scale"].get<float>())) : glm::mat4(1.0f);

        scene.transforms.push_back(translation * rotation * scale);
    }
    return scene;
}
} // namespace crisp