#include <Crisp/Scenes/RayTracingSceneParser.hpp>

#include <cmath>
#include <stdexcept>

namespace crisp {
namespace {
constexpr int32_t kBrdfLambertian = 0;
constexpr int32_t kBrdfDielectric = 1;
constexpr int32_t kBrdfMirror = 2;
constexpr int32_t kBrdfMicrofacet = 3;
constexpr int32_t kBrdfOrenNayar = 4;
constexpr int32_t kBrdfSmoothConductor = 5;
constexpr int32_t kBrdfRoughConductor = 6;
constexpr int32_t kBrdfRoughDielectric = 7;

constexpr int32_t kMicrofacetGgx = 0;
constexpr int32_t kMicrofacetBeckmann = 1;

constexpr int32_t kLightArea = 0;

BrdfParameters createLambertianBrdf(glm::vec3 albedo) {
    return {
        .albedo = albedo,
        .type = kBrdfLambertian,
    };
}

void validateDielectricIors(const float intIor, const float extIor) {
    if (intIor <= 0.0f || extIor <= 0.0f || intIor == extIor) {
        throw std::invalid_argument("Interior and exterior IORs must be positive and differ");
    }
}

BrdfParameters createDielectricBrdf(const float intIor, const float extIor) {
    validateDielectricIors(intIor, extIor);
    return {
        .type = kBrdfDielectric,
        .intIor = intIor,
        .extIor = extIor,
    };
}

BrdfParameters createMirrorBrdf() {
    return {
        .type = kBrdfMirror,
    };
}

BrdfParameters createOrenNayarBrdf(const glm::vec3 reflectance, const float roughnessDegrees) {
    return {
        .albedo = reflectance,
        .type = kBrdfOrenNayar,
        .roughness = glm::radians(glm::clamp(roughnessDegrees, 0.0f, 90.0f)),
    };
}

BrdfParameters createSmoothConductorBrdf(const std::string& iorPreset) {
    const auto ior = Fresnel::getComplexIOR(iorPreset);
    return {
        .type = kBrdfSmoothConductor,
        .complexIorEta = {ior.eta.r, ior.eta.g, ior.eta.b},
        .complexIorK = {ior.k.r, ior.k.g, ior.k.b},
    };
}

BrdfParameters createRoughConductorBrdf(const std::string& iorPreset, const int32_t microfacetType, const float alpha) {
    const auto ior = Fresnel::getComplexIOR(iorPreset);
    return {
        .type = kBrdfRoughConductor,
        .microfacetType = microfacetType,
        .complexIorEta = {ior.eta.r, ior.eta.g, ior.eta.b},
        .microfacetAlpha = glm::clamp(alpha, 1e-4f, 1.0f),
        .complexIorK = {ior.k.r, ior.k.g, ior.k.b},
    };
}

BrdfParameters createRoughDielectricBrdf(
    const float intIor, const float extIor, const int32_t microfacetType, const float alpha) {
    validateDielectricIors(intIor, extIor);
    return {
        .type = kBrdfRoughDielectric,
        .intIor = intIor,
        .extIor = extIor,
        .microfacetType = microfacetType,
        .microfacetAlpha = glm::clamp(alpha, 1e-4f, 1.0f),
    };
}

int32_t parseMicrofacetType(const std::string_view type) {
    if (type == "ggx") {
        return kMicrofacetGgx;
    }
    if (type == "beckmann") {
        return kMicrofacetBeckmann;
    }
    throw std::invalid_argument("Unsupported microfacet distribution: " + std::string(type));
}

BrdfParameters parseBrdfParameters(const nlohmann::json& brdf) {
    const auto& type{brdf["type"]};
    if (type == "lambertian") {
        return createLambertianBrdf(parseVec3(brdf["reflectance"]));
    }
    if (type == "oren-nayar") {
        return createOrenNayarBrdf(parseVec3(brdf["reflectance"]), brdf.value("roughnessDegrees", 0.0f));
    }
    if (type == "smooth-conductor") {
        return createSmoothConductorBrdf(brdf.value("conductorIorPreset", std::string("Au")));
    }
    if (type == "rough-conductor") {
        return createRoughConductorBrdf(
            brdf.value("conductorIorPreset", std::string("Au")),
            parseMicrofacetType(brdf.value("microfacetDistribution", std::string("beckmann"))),
            brdf.value("microfacetAlpha", 0.1f));
    }
    if (type == "rough-dielectric") {
        return createRoughDielectricBrdf(
            brdf.value("interiorIor", Fresnel::getIOR(IndexOfRefraction::Glass)),
            brdf.value("exteriorIor", Fresnel::getIOR(IndexOfRefraction::Air)),
            parseMicrofacetType(brdf.value("microfacetDistribution", std::string("beckmann"))),
            brdf.value("microfacetAlpha", 0.1f));
    }
    if (type == "dielectric") {
        return createDielectricBrdf(
            brdf.value("interiorIor", Fresnel::getIOR(IndexOfRefraction::Glass)),
            brdf.value("exteriorIor", Fresnel::getIOR(IndexOfRefraction::Air)));
    }
    if (type == "mirror") {
        return createMirrorBrdf();
    }
    if (type == "microfacet") {
        return createMicrofacetBrdf(
            parseVec3(brdf["diffuseReflectance"]),
            brdf.value("microfacetAlpha", 0.1f),
            parseMicrofacetType(brdf.value("microfacetDistribution", std::string("ggx"))));
    }
    return createLambertianBrdf(glm::vec3(1.0, 1.0, 0.0));
}

MaterialTextureDescription parseReflectanceTexture(const nlohmann::json& texture) {
    if (!texture.is_object()) {
        throw std::invalid_argument("reflectanceTexture must be an object");
    }
    const std::string type = texture.value("type", std::string{});
    if (type != "bitmap") {
        throw std::invalid_argument("Unsupported GPU reflectance texture type: " + type);
    }
    const std::string filename = texture.value("filename", std::string{});
    if (filename.empty()) {
        throw std::invalid_argument("Bitmap reflectanceTexture requires a filename");
    }
    return {.filename = filename};
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

BrdfParameters createMicrofacetBrdf(const glm::vec3 kd, const float alpha, const int32_t microfacetType) {
    return {
        .type = kBrdfMicrofacet,
        .intIor = Fresnel::getIOR(IndexOfRefraction::Glass),
        .microfacetType = microfacetType,
        .kd = kd,
        .ks = 1.0f - std::max(kd.x, std::max(kd.y, kd.z)),
        .microfacetAlpha = glm::clamp(alpha, 1e-4f, 1.0f),
    };
}

glm::vec3 parseVec3(const nlohmann::json& json) {
    return {json[0].get<float>(), json[1].get<float>(), json[2].get<float>()};
}

SceneDescription parseSceneDescription(const nlohmann::json& shapeList, const nlohmann::json& lightList) {
    SceneDescription scene{};
    for (const auto& shape : shapeList) {
        if (shape.value("type", std::string("mesh")) == "sphere") {
            scene.meshFilenames.emplace_back("sphere.obj");
        } else {
            scene.meshFilenames.push_back(shape["filename"]);
        }

        auto material = parseBrdfParameters(shape["bsdf"]);
        if (shape["bsdf"].contains("reflectanceTexture")) {
            if (material.type != kBrdfLambertian && material.type != kBrdfOrenNayar) {
                throw std::invalid_argument("reflectanceTexture is only supported by diffuse GPU materials");
            }
            material.reflectanceTexture = static_cast<int32_t>(scene.materialTextures.size());
            scene.materialTextures.push_back(parseReflectanceTexture(shape["bsdf"]["reflectanceTexture"]));
        }
        scene.brdfs.push_back(material);

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

    if (!lightList.is_null()) {
        if (!lightList.is_array()) {
            throw std::invalid_argument("Scene lights must be an array");
        }
        for (const auto& light : lightList) {
            const std::string type = light.value("type", std::string{});
            if (type != "environment") {
                throw std::invalid_argument("Unsupported standalone light type: " + type);
            }
            if (scene.environment.has_value()) {
                throw std::invalid_argument("Only one environment light is supported");
            }
            const std::string filename = light.value("filename", std::string{});
            const float radianceScale = light.value("radianceScale", 1.0f);
            if (filename.empty()) {
                throw std::invalid_argument("Environment light requires a filename");
            }
            if (!std::isfinite(radianceScale) || radianceScale < 0.0f) {
                throw std::invalid_argument("Environment radianceScale must be finite and non-negative");
            }
            scene.environment = EnvironmentLightDescription{filename, radianceScale};
        }
    }
    return scene;
}
} // namespace crisp
