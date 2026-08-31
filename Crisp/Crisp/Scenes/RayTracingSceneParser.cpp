#include <Crisp/Scenes/RayTracingSceneParser.hpp>

#include <cmath>
#include <limits>
#include <ranges>

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

BrdfParameters createLambertianBrdf(glm::vec3 albedo) {
    return {
        .albedo = albedo,
        .type = kBrdfLambertian,
    };
}

Result<> validateDielectricIors(const float intIor, const float extIor) {
    if (!std::isfinite(intIor) || !std::isfinite(extIor) || intIor <= 0.0f || extIor <= 0.0f || intIor == extIor) {
        return resultError("Interior and exterior IORs must be finite, positive, and different");
    }
    return {};
}

Result<BrdfParameters> createDielectricBrdf(const float intIor, const float extIor) {
    CRISP_TRY(validateDielectricIors(intIor, extIor));
    return BrdfParameters{
        .type = kBrdfDielectric,
        .intIor = intIor,
        .extIor = extIor,
    };
}

BrdfParameters createMirrorBrdf() {
    return BrdfParameters{
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

Result<BrdfParameters> createRoughDielectricBrdf(
    const float intIor, const float extIor, const int32_t microfacetType, const float alpha) {
    CRISP_TRY(validateDielectricIors(intIor, extIor));
    return BrdfParameters{
        .type = kBrdfRoughDielectric,
        .intIor = intIor,
        .extIor = extIor,
        .microfacetType = microfacetType,
        .microfacetAlpha = glm::clamp(alpha, 1e-4f, 1.0f),
    };
}

Result<int32_t> parseMicrofacetType(const std::string_view type) {
    if (type == "ggx") {
        return kMicrofacetGgx;
    }
    if (type == "beckmann") {
        return kMicrofacetBeckmann;
    }
    return resultError("Unsupported microfacet distribution: {}", type);
}

Result<BrdfParameters> parseBrdfParameters(const nlohmann::json& brdf) {
    if (!brdf.is_object() || !brdf.contains("type") || !brdf["type"].is_string()) {
        return resultError("BSDF must be an object with a string 'type' field");
    }
    const auto& type{brdf["type"]};
    if (type == "lambertian") {
        CRISP_TRY(const auto reflectance, parseVec3(brdf["reflectance"]));
        return createLambertianBrdf(reflectance);
    }
    if (type == "oren-nayar") {
        CRISP_TRY(const auto reflectance, parseVec3(brdf["reflectance"]));
        return createOrenNayarBrdf(reflectance, brdf.value("roughnessDegrees", 0.0f));
    }
    if (type == "smooth-conductor") {
        return createSmoothConductorBrdf(brdf.value("conductorIorPreset", std::string("Au")));
    }
    if (type == "rough-conductor") {
        CRISP_TRY(
            const auto microfacetType,
            parseMicrofacetType(brdf.value("microfacetDistribution", std::string("beckmann"))));
        return createRoughConductorBrdf(
            brdf.value("conductorIorPreset", std::string("Au")),
            microfacetType,
            brdf.value("microfacetAlpha", 0.1f));
    }
    if (type == "rough-dielectric") {
        CRISP_TRY(
            const auto microfacetType,
            parseMicrofacetType(brdf.value("microfacetDistribution", std::string("beckmann"))));
        return createRoughDielectricBrdf(
            brdf.value("interiorIor", Fresnel::getIOR(IndexOfRefraction::Glass)),
            brdf.value("exteriorIor", Fresnel::getIOR(IndexOfRefraction::Air)),
            microfacetType,
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
        CRISP_TRY(const auto diffuseReflectance, parseVec3(brdf["diffuseReflectance"]));
        CRISP_TRY(
            const auto microfacetType,
            parseMicrofacetType(brdf.value("microfacetDistribution", std::string("ggx"))));
        return createMicrofacetBrdf(
            diffuseReflectance, brdf.value("microfacetAlpha", 0.1f), microfacetType);
    }
    return resultError("Unsupported GPU BSDF type: {}", type.get<std::string>());
}

Result<MaterialTextureDescription> parseReflectanceTexture(const nlohmann::json& texture) {
    if (!texture.is_object()) {
        return resultError("reflectanceTexture must be an object");
    }
    const std::string type = texture.value("type", std::string{});
    if (type != "bitmap") {
        return resultError("Unsupported GPU reflectance texture type: {}", type);
    }
    const std::string filename = texture.value("filename", std::string{});
    if (filename.empty()) {
        return resultError("Bitmap reflectanceTexture requires a filename");
    }
    return MaterialTextureDescription{.filename = filename};
}

Result<glm::mat4> parseTransform(const nlohmann::json& shape) {
    if (shape.value("type", std::string("mesh")) == "sphere") {
        CRISP_TRY(const auto center, parseVec3(shape["center"]));
        const float radius = shape["radius"].get<float>();
        if (!std::isfinite(radius) || radius <= 0.0f) {
            return resultError("Sphere radius must be finite and positive");
        }
        return glm::translate(center) * glm::scale(glm::vec3(radius));
    }

    glm::mat4 transform(1.0f);
    if (shape.contains("toWorld")) {
        for (const auto& operation : shape["toWorld"]) {
            if (operation.contains("translation")) {
                CRISP_TRY(const auto translation, parseVec3(operation["translation"]));
                transform = glm::translate(translation) * transform;
            }
            if (operation.contains("scale")) {
                CRISP_TRY(const auto scale, parseVec3(operation["scale"]));
                transform = glm::scale(scale) * transform;
            }
            if (operation.contains("rotation")) {
                const auto& rotation = operation["rotation"];
                CRISP_TRY(const auto axis, parseVec3(rotation["axis"]));
                if (glm::length2(axis) == 0.0f) {
                    return resultError("Rotation axis must be non-zero");
                }
                transform =
                    glm::rotate(
                        glm::radians(rotation["angleDegrees"].get<float>()), glm::normalize(axis)) *
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

Result<glm::vec3> parseVec3(const nlohmann::json& json) {
    if (!json.is_array() || json.size() != 3 || !json[0].is_number() || !json[1].is_number() ||
        !json[2].is_number()) {
        return resultError("Expected an array containing exactly three numbers, got {}", json.dump());
    }

    const glm::vec3 value{json[0].get<float>(), json[1].get<float>(), json[2].get<float>()};
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
        return resultError("Vector components must be finite, got {}", json.dump());
    }
    return value;
}

Result<RayTracingRenderSettings> parseRayTracingRenderSettings(const nlohmann::json& json) {
    try {
        if (!json.is_object()) {
            return resultError("GPU path-tracing scene must be an object");
        }
        if (!json.contains("integrator") || !json["integrator"].is_object()) {
            return resultError("GPU path-tracing scene requires an integrator object");
        }
        if (!json.contains("sampler") || !json["sampler"].is_object()) {
            return resultError("GPU path-tracing scene requires a sampler object");
        }
        if (!json.contains("camera") || !json["camera"].is_object()) {
            return resultError("GPU path-tracing scene requires a camera object");
        }

        RayTracingRenderSettings settings{};
        const auto& integrator = json["integrator"];
        const auto& sampler = json["sampler"];
        const auto& camera = json["camera"];

        if (integrator.contains("maxDepth") && !integrator["maxDepth"].is_number_integer()) {
            return resultError("Integrator maxDepth must be an integer");
        }
        settings.maxDepth = integrator.value("maxDepth", settings.maxDepth);
        if (settings.maxDepth <= 0) {
            return resultError("Integrator maxDepth must be greater than zero");
        }

        if (!sampler.contains("samplesPerPixel") || !sampler["samplesPerPixel"].is_number_integer()) {
            return resultError("Sampler samplesPerPixel must be an integer");
        }
        settings.samplesPerPixel = sampler["samplesPerPixel"].get<int32_t>();
        if (settings.samplesPerPixel <= 0) {
            return resultError("Sampler samplesPerPixel must be greater than zero");
        }
        if (sampler.contains("seed")) {
            const auto& seed = sampler["seed"];
            if ((!seed.is_number_integer() && !seed.is_number_unsigned()) || seed.get<int64_t>() < 0 ||
                seed.get<uint64_t>() > std::numeric_limits<uint32_t>::max()) {
                return resultError("Sampler seed must be an integer in the uint32 range");
            }
            settings.seed = seed.get<uint32_t>();
        }

        if (!camera.contains("imageSize") || !camera["imageSize"].is_array() || camera["imageSize"].size() != 2 ||
            !camera["imageSize"][0].is_number_integer() || !camera["imageSize"][1].is_number_integer()) {
            return resultError("Camera imageSize must contain exactly two integers");
        }
        settings.resolution = {camera["imageSize"][0].get<int32_t>(), camera["imageSize"][1].get<int32_t>()};
        if (settings.resolution.x <= 0 || settings.resolution.y <= 0) {
            return resultError("Camera imageSize components must be greater than zero");
        }

        if (!camera.contains("position") || !camera.contains("target") || !camera.contains("up")) {
            return resultError("Camera requires position, target, and up vectors");
        }
        CRISP_TRY(settings.cameraPosition, parseVec3(camera["position"]));
        CRISP_TRY(settings.cameraTarget, parseVec3(camera["target"]));
        CRISP_TRY(settings.cameraUp, parseVec3(camera["up"]));
        const glm::vec3 viewDirection = settings.cameraTarget - settings.cameraPosition;
        if (glm::length2(viewDirection) <= 1e-12f) {
            return resultError("Camera position and target must be different");
        }
        if (glm::length2(settings.cameraUp) <= 1e-12f ||
            glm::length2(glm::cross(viewDirection, settings.cameraUp)) <= 1e-12f) {
            return resultError("Camera up must be non-zero and not parallel to the view direction");
        }

        if (!camera.contains("fovY") || !camera["fovY"].is_number()) {
            return resultError("Camera fovY must be numeric");
        }
        settings.verticalFov = camera["fovY"].get<float>();
        if (!std::isfinite(settings.verticalFov) || settings.verticalFov <= 0.0f || settings.verticalFov >= 180.0f) {
            return resultError("Camera fovY must be finite and between zero and 180 degrees");
        }

        settings.zNear = camera.value("zNear", settings.zNear);
        settings.zFar = camera.value("zFar", settings.zFar);
        if (!std::isfinite(settings.zNear) || !std::isfinite(settings.zFar) || settings.zNear <= 0.0f ||
            settings.zFar <= settings.zNear) {
            return resultError("Camera depth range must be finite and satisfy 0 < zNear < zFar");
        }

        if (camera.contains("reconstructionFilter")) {
            const auto& filter = camera["reconstructionFilter"];
            if (!filter.is_object() || !filter.contains("type") || !filter["type"].is_string()) {
                return resultError("Camera reconstructionFilter must be an object with a string type");
            }
            const std::string type = filter["type"].get<std::string>();
            if (type != "box") {
                return resultError("Unsupported GPU reconstruction filter: {}", type);
            }
            settings.reconstructionFilter = ReconstructionFilterType::Box;
        }

        return settings;
    } catch (const nlohmann::json::exception& exception) {
        return resultError("Invalid GPU path-tracing render settings: {}", exception.what());
    }
}

Result<SceneDescription> parseSceneDescription(const nlohmann::json& shapeList, const nlohmann::json& lightList) {
    try {
        if (!shapeList.is_array()) {
            return resultError("Scene shapes must be an array");
        }

        SceneDescription scene{};
        for (auto&& [shapeIndex, shape] : std::views::enumerate(shapeList)) {
            if (!shape.is_object()) {
                return resultError("Shape {} must be an object", shapeIndex);
            }

            if (shape.value("type", std::string("mesh")) == "sphere") {
                scene.meshFilenames.emplace_back("sphere.obj");
            } else {
                const std::string filename = shape.value("filename", std::string{});
                if (filename.empty()) {
                    return resultError("Mesh shape {} requires a filename", shapeIndex);
                }
                scene.meshFilenames.push_back(filename);
            }

            CRISP_TRY(auto material, parseBrdfParameters(shape["bsdf"]));
            if (shape["bsdf"].contains("reflectanceTexture")) {
                if (material.type != kBrdfLambertian && material.type != kBrdfOrenNayar) {
                    return resultError("Shape {} uses reflectanceTexture on a non-diffuse GPU material", shapeIndex);
                }
                CRISP_TRY(auto texture, parseReflectanceTexture(shape["bsdf"]["reflectanceTexture"]));
                material.reflectanceTexture = static_cast<int32_t>(scene.materialTextures.size());
                scene.materialTextures.push_back(std::move(texture));
            }
            scene.brdfs.push_back(material);

            if (shape.contains("light")) {
                if (!shape["light"].is_object()) {
                    return resultError("Area light on shape {} must be an object", shapeIndex);
                }
                if (shape["light"].value("type", std::string{}) != "area") {
                    return resultError("Shape {} only supports an area light", shapeIndex);
                }
                CRISP_TRY(const auto radiance, parseVec3(shape["light"]["radiance"]));
                if (glm::any(glm::lessThan(radiance, glm::vec3(0.0f)))) {
                    return resultError("Area-light radiance on shape {} must be non-negative", shapeIndex);
                }

                const auto lightIdx = static_cast<int32_t>(scene.lights.size());
                scene.props.push_back(
                    {.materialId = static_cast<int32_t>(scene.brdfs.size() - 1), .lightId = lightIdx});

                const auto meshIdx = static_cast<int32_t>(scene.meshFilenames.size() - 1);
                scene.lights.push_back({
                    .type = kLightArea,
                    .meshId = meshIdx,
                    .emission = radiance,
                });
            } else {
                scene.props.push_back({.materialId = static_cast<int32_t>(scene.brdfs.size() - 1), .lightId = -1});
            }

            CRISP_TRY(auto transform, parseTransform(shape));
            scene.transforms.push_back(std::move(transform));
        }

        if (!lightList.is_null()) {
            if (!lightList.is_array()) {
                return resultError("Scene lights must be an array");
            }
            for (auto&& [lightIndex, light] : std::views::enumerate(lightList)) {
                if (!light.is_object()) {
                    return resultError("Standalone light {} must be an object", lightIndex);
                }
                const std::string type = light.value("type", std::string{});
                if (type == "point") {
                    if (!light.contains("position")) {
                        return resultError("Point light {} requires a position", lightIndex);
                    }
                    if (!light.contains("power")) {
                        return resultError("Point light {} requires power", lightIndex);
                    }
                    CRISP_TRY(const auto position, parseVec3(light["position"]));
                    CRISP_TRY(const auto power, parseVec3(light["power"]));
                    if (glm::any(glm::lessThan(power, glm::vec3(0.0f)))) {
                        return resultError("Point-light power {} must be non-negative", lightIndex);
                    }
                    scene.lights.push_back({
                        .type = kLightPoint,
                        .emission = power,
                        .positionOrDirection = position,
                    });
                    continue;
                }
                if (type == "directional") {
                    if (!light.contains("direction")) {
                        return resultError("Directional light {} requires a direction", lightIndex);
                    }
                    if (!light.contains("power")) {
                        return resultError("Directional light {} requires power", lightIndex);
                    }
                    CRISP_TRY(auto direction, parseVec3(light["direction"]));
                    CRISP_TRY(const auto power, parseVec3(light["power"]));
                    const float directionLengthSquared = glm::dot(direction, direction);
                    if (!std::isfinite(directionLengthSquared) || directionLengthSquared <= 0.0f) {
                        return resultError(
                            "Directional-light direction {} must have finite, non-zero length", lightIndex);
                    }
                    if (glm::any(glm::lessThan(power, glm::vec3(0.0f)))) {
                        return resultError("Directional-light power {} must be non-negative", lightIndex);
                    }
                    direction /= std::sqrt(directionLengthSquared);
                    scene.lights.push_back({
                        .type = kLightDirectional,
                        .emission = power,
                        .positionOrDirection = direction,
                    });
                    continue;
                }
                if (type != "environment") {
                    return resultError("Unsupported standalone light type: {}", type);
                }
                if (scene.environment.has_value()) {
                    return resultError("Only one environment light is supported");
                }
                const bool hasFilename = light.contains("filename");
                const bool hasRadiance = light.contains("radiance");
                if (hasFilename == hasRadiance) {
                    return resultError("Environment light requires exactly one of filename or radiance");
                }

                std::optional<std::string> filename;
                std::optional<glm::vec3> radiance;
                if (hasFilename) {
                    filename = light["filename"].get<std::string>();
                    if (filename->empty()) {
                        return resultError("Environment light filename must not be empty");
                    }
                } else {
                    CRISP_TRY(auto parsedRadiance, parseVec3(light["radiance"]));
                    if (glm::any(glm::lessThan(parsedRadiance, glm::vec3(0.0f)))) {
                        return resultError("Environment radiance must be non-negative");
                    }
                    radiance = parsedRadiance;
                }
                const float radianceScale = light.value("radianceScale", 1.0f);
                if (!std::isfinite(radianceScale) || radianceScale < 0.0f) {
                    return resultError("Environment radianceScale must be finite and non-negative");
                }
                scene.environment = EnvironmentLightDescription{
                    .filename = std::move(filename),
                    .radiance = radiance,
                    .radianceScale = radianceScale,
                };
            }
        }
        return scene;
    } catch (const nlohmann::json::exception& exception) {
        return resultError("Invalid GPU path-tracing scene JSON: {}", exception.what());
    }
}
} // namespace crisp
