#include <Crisp/PathTracer/Core/JsonSceneParser.hpp>

#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/PathTracer/BSDFs/BSDFFactory.hpp>
#include <Crisp/PathTracer/BSSRDFs/BSSRDFFactory.hpp>
#include <Crisp/PathTracer/Cameras/CameraFactory.hpp>
#include <Crisp/PathTracer/Core/Scene.hpp>
#include <Crisp/PathTracer/Core/VariantMap.hpp>
#include <Crisp/PathTracer/Integrators/IntegratorFactory.hpp>
#include <Crisp/PathTracer/Lights/LightFactory.hpp>
#include <Crisp/PathTracer/Samplers/SamplerFactory.hpp>
#include <Crisp/PathTracer/Shapes/ShapeFactory.hpp>
#include <Crisp/PathTracer/Textures/TextureFactory.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace crisp {
namespace {
using Json = nlohmann::json;

enum class ParameterType {
    Boolean,
    Integer,
    Float,
    String,
    Vec2,
    IVec2,
    Vec3,
    Spectrum,
    Transform,
};

struct ParameterSpec {
    std::string_view name;
    ParameterType type;
};

constexpr std::array<ParameterSpec, 0> kNoParameters{};
constexpr std::array<std::string_view, 0> kNoNestedFields{};

constexpr std::array kAmbientOcclusionParameters{
    ParameterSpec{"maxDistance", ParameterType::Float},
};
constexpr std::array kSamplerParameters{
    ParameterSpec{"samplesPerPixel", ParameterType::Integer},
};
constexpr std::array kCameraParameters{
    ParameterSpec{"imageSize", ParameterType::IVec2},
    ParameterSpec{"fovY", ParameterType::Float},
    ParameterSpec{"zNear", ParameterType::Float},
    ParameterSpec{"zFar", ParameterType::Float},
    ParameterSpec{"position", ParameterType::Vec3},
    ParameterSpec{"target", ParameterType::Vec3},
    ParameterSpec{"up", ParameterType::Vec3},
    ParameterSpec{"mirrorHorizontally", ParameterType::Boolean},
};
constexpr std::array kMeshParameters{
    ParameterSpec{"filename", ParameterType::String},
    ParameterSpec{"toWorld", ParameterType::Transform},
};
constexpr std::array kSphereParameters{
    ParameterSpec{"center", ParameterType::Vec3},
    ParameterSpec{"radius", ParameterType::Float},
};
constexpr std::array kLambertianParameters{
    ParameterSpec{"reflectance", ParameterType::Spectrum},
};
constexpr std::array kOrenNayarParameters{
    ParameterSpec{"reflectance", ParameterType::Spectrum},
    ParameterSpec{"roughnessDegrees", ParameterType::Float},
};
constexpr std::array kDielectricParameters{
    ParameterSpec{"interiorIor", ParameterType::Float},
    ParameterSpec{"exteriorIor", ParameterType::Float},
};
constexpr std::array kMicrofacetParameters{
    ParameterSpec{"microfacetDistribution", ParameterType::String},
    ParameterSpec{"microfacetAlpha", ParameterType::Float},
    ParameterSpec{"interiorIor", ParameterType::Float},
    ParameterSpec{"exteriorIor", ParameterType::Float},
    ParameterSpec{"diffuseReflectance", ParameterType::Spectrum},
};
constexpr std::array kRoughDielectricParameters{
    ParameterSpec{"microfacetDistribution", ParameterType::String},
    ParameterSpec{"microfacetAlpha", ParameterType::Float},
    ParameterSpec{"interiorIor", ParameterType::Float},
    ParameterSpec{"exteriorIor", ParameterType::Float},
};
constexpr std::array kRoughConductorParameters{
    ParameterSpec{"conductorIorPreset", ParameterType::String},
    ParameterSpec{"microfacetDistribution", ParameterType::String},
    ParameterSpec{"microfacetAlpha", ParameterType::Float},
};
constexpr std::array kSmoothConductorParameters{
    ParameterSpec{"conductorIorPreset", ParameterType::String},
};
constexpr std::array kPointLightParameters{
    ParameterSpec{"position", ParameterType::Vec3},
    ParameterSpec{"power", ParameterType::Spectrum},
};
constexpr std::array kDirectionalLightParameters{
    ParameterSpec{"direction", ParameterType::Vec3},
    ParameterSpec{"power", ParameterType::Spectrum},
};
constexpr std::array kAreaLightParameters{
    ParameterSpec{"radiance", ParameterType::Spectrum},
};
constexpr std::array kEnvironmentLightParameters{
    ParameterSpec{"filename", ParameterType::String},
    ParameterSpec{"radianceScale", ParameterType::Float},
};
constexpr std::array<std::string_view, 3> kShapeNestedFields{"bsdf", "bssrdf", "light"};
constexpr std::array<std::string_view, 1> kReflectanceTextureNestedFields{"reflectanceTexture"};

const Json* findChild(const Json& node, const std::string_view name) {
    const auto iter = node.find(name);
    return iter == node.end() ? nullptr : &*iter;
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::invalid_argument(message);
    }
}

void requireNumberArray(const Json& value, const size_t size, const std::string_view fieldName) {
    require(value.is_array() && value.size() == size, "Field '" + std::string(fieldName) + "' has invalid size");
    require(
        std::ranges::all_of(value, [](const Json& entry) { return entry.is_number(); }),
        "Field '" + std::string(fieldName) + "' must contain only numbers");
}

glm::vec2 parseVec2(const Json& value, const std::string_view fieldName) {
    requireNumberArray(value, 2, fieldName);
    return {value.at(0).get<float>(), value.at(1).get<float>()};
}

glm::ivec2 parseIVec2(const Json& value, const std::string_view fieldName) {
    requireNumberArray(value, 2, fieldName);
    require(
        std::ranges::all_of(value, [](const Json& entry) { return entry.is_number_integer(); }),
        "Field '" + std::string(fieldName) + "' must contain only integers");
    return {value.at(0).get<int>(), value.at(1).get<int>()};
}

glm::vec3 parseVec3(const Json& value, const std::string_view fieldName) {
    requireNumberArray(value, 3, fieldName);
    return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

Spectrum parseSpectrum(const Json& value, const std::string_view fieldName) {
    const auto rgb = parseVec3(value, fieldName);
    return {rgb.r, rgb.g, rgb.b};
}

void applyTransformOperation(Transform& transform, const Json& operation) {
    require(operation.is_object() && operation.size() == 1, "Each transform operation must contain one field");

    const auto iter = operation.begin();
    const auto& name = iter.key();
    const auto& value = iter.value();
    if (name == "translation") {
        transform = Transform::createTranslation(parseVec3(value, name)) * transform;
    } else if (name == "scale") {
        transform = Transform::createScale(parseVec3(value, name)) * transform;
    } else if (name == "rotation") {
        require(value.is_object() && value.size() == 2, "Rotation must contain axis and angleDegrees");
        require(value.contains("axis") && value.contains("angleDegrees"), "Rotation requires axis and angleDegrees");
        require(value.at("angleDegrees").is_number(), "Rotation angleDegrees must be numeric");
        transform =
            Transform::createRotation(
                value.at("angleDegrees").get<float>(), parseVec3(value.at("axis"), "rotation.axis")) *
            transform;
    } else {
        throw std::invalid_argument("Unknown transform operation: " + name);
    }
}

Transform parseTransform(const Json& value) {
    require(value.is_array(), "Field 'toWorld' must be an array of transform operations");
    Transform transform;
    for (const auto& operation : value) {
        applyTransformOperation(transform, operation);
    }
    return transform;
}

bool contains(std::span<const std::string_view> values, const std::string_view value) {
    return std::ranges::find(values, value) != values.end();
}

void parseParameter(VariantMap& params, const ParameterSpec& spec, const Json& value) {
    const auto name = std::string(spec.name);
    switch (spec.type) {
    case ParameterType::Boolean:
        require(value.is_boolean(), "Field '" + name + "' must be boolean");
        params.insert(name, value.get<bool>());
        break;
    case ParameterType::Integer:
        require(value.is_number_integer(), "Field '" + name + "' must be an integer");
        params.insert(name, value.get<int>());
        break;
    case ParameterType::Float:
        require(value.is_number(), "Field '" + name + "' must be numeric");
        params.insert(name, value.get<float>());
        break;
    case ParameterType::String:
        require(value.is_string(), "Field '" + name + "' must be a string");
        params.insert(name, value.get<std::string>());
        break;
    case ParameterType::Vec2:
        params.insert(name, parseVec2(value, name));
        break;
    case ParameterType::IVec2:
        params.insert(name, parseIVec2(value, name));
        break;
    case ParameterType::Vec3:
        params.insert(name, parseVec3(value, name));
        break;
    case ParameterType::Spectrum:
        params.insert(name, parseSpectrum(value, name));
        break;
    case ParameterType::Transform:
        params.insert(name, parseTransform(value));
        break;
    }
}

void parseParameters(
    VariantMap& params,
    const Json& node,
    const std::span<const ParameterSpec> parameterSpecs,
    const std::span<const std::string_view> nestedFields = kNoNestedFields) {
    require(node.is_object(), "Scene component must be a JSON object");
    for (const auto& [name, value] : node.items()) {
        if (name == "type") {
            require(value.is_string(), "Field 'type' must be a string");
            continue;
        }
        if (contains(nestedFields, name)) {
            continue;
        }

        const auto iter = std::ranges::find(parameterSpecs, name, &ParameterSpec::name);
        if (iter == parameterSpecs.end()) {
            throw std::invalid_argument("Unknown field '" + name + "'");
        }
        parseParameter(params, *iter, value);
    }
}

template <typename Type>
std::span<const ParameterSpec> getParameterSpecs(const std::string_view type) {
    if constexpr (std::is_same_v<Type, Integrator>) {
        if (type == "ambient-occlusion") {
            return kAmbientOcclusionParameters;
        }
        return kNoParameters;
    } else if constexpr (std::is_same_v<Type, Sampler>) {
        return kSamplerParameters;
    } else if constexpr (std::is_same_v<Type, Camera>) {
        return kCameraParameters;
    } else if constexpr (std::is_same_v<Type, Shape>) {
        return type == "sphere" ? std::span<const ParameterSpec>(kSphereParameters)
                                : std::span<const ParameterSpec>(kMeshParameters);
    } else if constexpr (std::is_same_v<Type, BSDF>) {
        if (type == "lambertian") {
            return kLambertianParameters;
        }
        if (type == "oren-nayar") {
            return kOrenNayarParameters;
        }
        if (type == "dielectric") {
            return kDielectricParameters;
        }
        if (type == "microfacet") {
            return kMicrofacetParameters;
        }
        if (type == "rough-dielectric") {
            return kRoughDielectricParameters;
        }
        if (type == "rough-conductor") {
            return kRoughConductorParameters;
        }
        if (type == "smooth-conductor") {
            return kSmoothConductorParameters;
        }
        return kNoParameters;
    } else if constexpr (std::is_same_v<Type, Light>) {
        if (type == "point") {
            return kPointLightParameters;
        }
        if (type == "directional") {
            return kDirectionalLightParameters;
        }
        if (type == "area") {
            return kAreaLightParameters;
        }
        if (type == "environment") {
            return kEnvironmentLightParameters;
        }
        return kNoParameters;
    } else {
        return kNoParameters;
    }
}

template <typename Type>
std::span<const std::string_view> getNestedFields(const std::string_view type) {
    if constexpr (std::is_same_v<Type, Shape>) {
        return kShapeNestedFields;
    } else if constexpr (std::is_same_v<Type, BSDF>) {
        if (type == "lambertian" || type == "oren-nayar") {
            return kReflectanceTextureNestedFields;
        }
        return kNoNestedFields;
    } else {
        return kNoNestedFields;
    }
}

template <typename Type, typename FactoryType>
std::unique_ptr<Type> create(const Json* node, const std::filesystem::path& meshDirectory = {}) {
    std::string type = "default";
    VariantMap params;
    if (node != nullptr) {
        type = node->value("type", type);
        parseParameters(params, *node, getParameterSpecs<Type>(type), getNestedFields<Type>(type));
    }

    if constexpr (std::is_same_v<Type, Shape>) {
        if (params.contains("filename")) {
            std::filesystem::path meshPath{params.get<std::string>("filename")};
            if (meshPath.is_relative()) {
                meshPath = meshDirectory / meshPath;
            }
            params.insert("filename", meshPath.string());
        }
    }
    return FactoryType::create(type, params);
}

template <typename DataType>
std::unique_ptr<Texture<DataType>> createTexture(const Json& node) {
    const std::string type = node.value("type", std::string("default"));
    constexpr auto valueType = std::is_same_v<DataType, Spectrum> ? ParameterType::Spectrum : ParameterType::Float;
    const std::array checkerboardParameters{
        ParameterSpec{"offset", ParameterType::Vec2},
        ParameterSpec{"tileSize", ParameterType::Vec2},
        ParameterSpec{"evenValue", valueType},
        ParameterSpec{"oddValue", valueType},
    };
    const std::array constantParameters{ParameterSpec{"value", valueType}};

    const std::span<const ParameterSpec> specs =
        type.starts_with("checkerboard") ? std::span<const ParameterSpec>(checkerboardParameters)
        : type.starts_with("constant")
            ? std::span<const ParameterSpec>(constantParameters)
            : std::span<const ParameterSpec>(kNoParameters);
    VariantMap params;
    parseParameters(params, node, specs);
    return TextureFactory::create<DataType>(type, params);
}

void validateSceneRoot(const Json& sceneNode) {
    static constexpr std::array<std::string_view, 5> kSceneFields{"integrator", "sampler", "camera", "shapes", "lights"};
    require(sceneNode.is_object(), "Scene JSON root must be an object");
    for (const auto& [name, value] : sceneNode.items()) {
        require(contains(kSceneFields, name), "Unknown scene field '" + name + "'");
        if (name == "shapes" || name == "lights") {
            require(value.is_array(), "Scene field '" + name + "' must be an array");
        } else {
            require(value.is_object(), "Scene field '" + name + "' must be an object");
        }
    }
}
} // namespace

Result<std::unique_ptr<pt::Scene>> JsonSceneParser::parse(
    const std::filesystem::path& sceneFilePath, const std::filesystem::path& meshDirectory) {
    try {
        CRISP_TRY(
            auto document,
            loadJsonFromFile(sceneFilePath),
            "Failed to load path-tracing scene at {}",
            sceneFilePath.string());

        validateSceneRoot(document);

        auto scene = std::make_unique<pt::Scene>();
        scene->setIntegrator(create<Integrator, IntegratorFactory>(findChild(document, "integrator")));
        scene->setSampler(create<Sampler, SamplerFactory>(findChild(document, "sampler")));
        scene->setCamera(create<Camera, CameraFactory>(findChild(document, "camera")));

        if (const auto* shapes = findChild(document, "shapes")) {
            for (const auto& shapeNode : *shapes) {
                std::unique_ptr<Light> light;
                if (const auto* lightNode = findChild(shapeNode, "light")) {
                    light = create<Light, LightFactory>(lightNode);
                }

                std::unique_ptr<BSSRDF> bssrdf;
                if (const auto* bssrdfNode = findChild(shapeNode, "bssrdf")) {
                    bssrdf = create<BSSRDF, BSSRDFFactory>(bssrdfNode);
                }

                const Json* bsdfNode = findChild(shapeNode, "bsdf");
                auto bsdf = create<BSDF, BSDFFactory>(bsdfNode);
                if (bsdfNode != nullptr) {
                    if (const auto* texture = findChild(*bsdfNode, "reflectanceTexture")) {
                        bsdf->setTexture(createTexture<Spectrum>(*texture));
                    }
                }

                auto shape = create<Shape, ShapeFactory>(&shapeNode, meshDirectory);
                shape->setBSSRDF(std::move(bssrdf));
                scene->addShape(std::move(shape), bsdf.get(), light.get());

                if (light) {
                    scene->addLight(std::move(light));
                }
                scene->addBSDF(std::move(bsdf));
            }
        }

        if (const auto* lights = findChild(document, "lights")) {
            for (const auto& lightNode : *lights) {
                if (lightNode.value("type", std::string("default")) == "environment") {
                    scene->addEnvironmentLight(create<Light, LightFactory>(&lightNode));
                } else {
                    scene->addLight(create<Light, LightFactory>(&lightNode));
                }
            }
        }

        scene->finishInitialization();
        return scene;
    } catch (const std::exception& exception) {
        return resultError("Failed to parse path-tracing scene at {}: {}", sceneFilePath.string(), exception.what());
    }
}
} // namespace crisp
