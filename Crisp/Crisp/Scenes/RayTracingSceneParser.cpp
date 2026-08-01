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
    if (type == "lambertian" || type == "oren-nayar") {
        return createLambertianBrdf(parseVec3(brdf["reflectance"]));
    }
    if (type == "dielectric") {
        return createDielectricBrdf(brdf.value("interiorIor", Fresnel::getIOR(IndexOfRefraction::Glass)));
    }
    if (type == "mirror") {
        return createMirrorBrdf();
    }
    if (type == "microfacet") {
        return createMicrofacetBrdf(parseVec3(brdf["diffuseReflectance"]), brdf.value("microfacetAlpha", 0.1f));
    }
    return createLambertianBrdf(glm::vec3(1.0, 1.0, 0.0));
}

glm::mat4 parseTransform(const nlohmann::json& shape) {
    if (shape.value("type", std::string("mesh")) == "sphere") {
        return glm::translate(parseVec3(shape["center"])) * glm::scale(glm::vec3(shape["radius"].get<float>()));
    }

    glm::mat4 transform(1.0f);
    if (shape.contains("toWorld")) {
        for (const auto& operation : shape["toWorld"]) {
            if (operation.contains("translation")) {
                transform = glm::translate(parseVec3(operation["translation"])) * transform;
            }
            if (operation.contains("scale")) {
                transform = glm::scale(parseVec3(operation["scale"])) * transform;
            }
            if (operation.contains("rotation")) {
                const auto& rotation = operation["rotation"];
                transform =
                    glm::rotate(
                        glm::radians(rotation["angleDegrees"].get<float>()),
                        glm::normalize(parseVec3(rotation["axis"]))) *
                    transform;
            }
        }
        return transform;
    }
    return transform;
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
        if (shape.value("type", std::string("mesh")) == "sphere") {
            scene.meshFilenames.emplace_back("sphere.obj");
        } else {
            scene.meshFilenames.push_back(shape["filename"]);
        }

        scene.brdfs.push_back(parseBrdfParameters(shape["bsdf"]));

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

        scene.transforms.push_back(parseTransform(shape));
    }
    return scene;
}
} // namespace crisp
