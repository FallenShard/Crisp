#include <Crisp/Mesh/Io/GltfLoader.hpp>

#include <ranges>

#pragma warning(push)
#pragma warning(disable : 4018) // Signed/unsigned comparison.
#pragma warning(disable : 4267) // Signed/unsigned comparison.
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>
#pragma warning(pop)

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Image/Io/Utils.hpp>

namespace crisp {
namespace {
const auto logger = createLoggerMt("GltfLoader");

constexpr uint32_t kRoughnessMapChannel{1};
constexpr uint32_t kMetallicMapChannel{2};

constexpr int32_t GltfInvalidIdx{-1};

constexpr bool isValidGltfIndex(const int32_t index) {
    return index != GltfInvalidIdx;
}

template <typename T>
concept ScalarAttrib = std::floating_point<T> || (std::integral<T> && sizeof(T) <= 4);

template <typename T>
struct IsGlmVec : public std::false_type {};

template <glm::length_t L, typename T>
struct IsGlmVec<glm::vec<L, T, glm::defaultp>> : public std::true_type {};

template <typename T>
struct IsGlmQuat : public std::false_type {};

template <typename T>
struct IsGlmQuat<glm::qua<T, glm::defaultp>> : public std::true_type {};

template <typename T>
concept GlmVector = IsGlmVec<T>::value;

template <typename T>
concept GlmQuaternion = IsGlmQuat<T>::value;

template <typename T>
concept GlmAttrib = GlmVector<T> || GlmQuaternion<T>;

template <typename T>
concept GltfAttrib = ScalarAttrib<T> || GlmAttrib<T>;

template <GltfAttrib T>
struct ComponentTypeHelper {
    using Type = typename T::value_type;
    static constexpr int32_t kCount = T::length();
};

template <ScalarAttrib T>
struct ComponentTypeHelper<T> {
    using Type = T;
    static constexpr int32_t kCount = 1;
};

template <GltfAttrib T>
using ComponentType = typename ComponentTypeHelper<T>::Type;

template <GltfAttrib T>
constexpr int32_t ComponentCount = ComponentTypeHelper<T>::kCount;

template <ScalarAttrib T>
int32_t determineGltfComponentType() {
    if constexpr (std::is_same_v<T, float>) {
        return TINYGLTF_COMPONENT_TYPE_FLOAT;
    } else if constexpr (std::is_same_v<T, double>) {
        return TINYGLTF_COMPONENT_TYPE_DOUBLE;
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        return TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
    } else if constexpr (std::is_same_v<T, uint16_t>) {
        return TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
    } else if constexpr (std::is_same_v<T, uint8_t>) {
        return TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    } else if constexpr (std::is_same_v<T, int32_t>) {
        return TINYGLTF_COMPONENT_TYPE_INT;
    } else if constexpr (std::is_same_v<T, int16_t>) {
        return TINYGLTF_COMPONENT_TYPE_SHORT;
    } else if constexpr (std::is_same_v<T, int8_t>) {
        return TINYGLTF_COMPONENT_TYPE_BYTE;
    } else {
        []<bool flag = false>() {
            static_assert(flag, "Encountered unknown type in determineGltfComponentType()");
        }
        ();
    }
}

Result<std::vector<glm::uvec3>> loadIndexBuffer(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
    const auto& bufferView{model.bufferViews.at(accessor.bufferView)};
    const auto& buffer{model.buffers.at(bufferView.buffer)};

    const int32_t componentByteSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const int32_t componentCount = tinygltf::GetNumComponentsInType(accessor.type);
    const size_t attributeByteSize = componentByteSize * componentCount;

    CRISP_CHECK_EQ(accessor.count % glm::uvec3::length(), 0);
    const size_t triangleCount = accessor.count / glm::uvec3::length();

    const size_t byteStride{bufferView.byteStride == 0 ? attributeByteSize : bufferView.byteStride};
    const size_t bufferRangeStart{bufferView.byteOffset + accessor.byteOffset};
    CRISP_CHECK(bufferRangeStart <= buffer.data.size());
    CRISP_CHECK(bufferRangeStart + accessor.count * byteStride <= buffer.data.size());

    std::vector<glm::uvec3> indices(triangleCount);
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT && byteStride == attributeByteSize) {
        std::memcpy(
            indices.data(), buffer.data.data() + bufferView.byteOffset, accessor.count * attributeByteSize); // NOLINT
    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        glm::u16vec3 temp{};
        for (size_t i = 0; i < indices.size(); ++i) {
            for (uint32_t j = 0; j < 3; ++j) {
                const size_t offset{bufferRangeStart + (i * 3 + j) * byteStride};
                std::memcpy(&temp[j], buffer.data.data() + offset, attributeByteSize); // NOLINT
            }
            indices[i] = glm::uvec3(temp.x, temp.y, temp.z);
        }
    } else {
        return resultError("Failed to parse index buffer!");
    }

    return indices;
}

template <GltfAttrib DstType, GltfAttrib SrcType = DstType>
Result<std::vector<DstType>> createBuffer(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
    const int32_t componentByteSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    CRISP_CHECK_EQ(accessor.componentType, determineGltfComponentType<ComponentType<SrcType>>());

    const int32_t componentCount = tinygltf::GetNumComponentsInType(accessor.type);
    CRISP_CHECK_EQ(componentCount, ComponentCount<SrcType>);

    const int32_t attributeByteSize = componentCount * componentByteSize;
    CRISP_CHECK_EQ(attributeByteSize, sizeof(SrcType));

    const auto& bufferView = model.bufferViews.at(accessor.bufferView);
    const auto& buffer = model.buffers.at(bufferView.buffer);

    const size_t byteStride = bufferView.byteStride == 0 ? attributeByteSize : bufferView.byteStride;
    const size_t bufferRangeStart = bufferView.byteOffset + accessor.byteOffset;
    CRISP_CHECK_LE(bufferRangeStart, buffer.data.size());
    CRISP_CHECK_LE(bufferRangeStart + accessor.count * byteStride, buffer.data.size());

    std::vector<DstType> attributes;
    attributes.reserve(accessor.count);

    if (static_cast<int32_t>(byteStride) == attributeByteSize && std::is_same_v<DstType, SrcType> &&
        std::is_trivially_copy_assignable_v<DstType>) {
        attributes.resize(accessor.count);
        std::memcpy(
            attributes.data(), buffer.data.data() + bufferRangeStart, sizeof(DstType) * accessor.count); // NOLINT
        return attributes;
    }

    SrcType temp{};
    for (size_t i = 0; i < accessor.count; ++i) {
        const size_t offset{bufferRangeStart + i * byteStride};
        std::memcpy(&temp, buffer.data.data() + offset, attributeByteSize); // NOLINT
        attributes.emplace_back(temp);
    }

    return attributes;
}

template <GltfAttrib DstType, GltfAttrib SrcType = DstType>
Result<std::vector<DstType>> createBuffer(
    const tinygltf::Model& model, const tinygltf::Primitive& primitive, const std::string& attrib) {
    if (!primitive.attributes.contains(attrib)) {
        return std::vector<DstType>{};
    }
    return createBuffer<DstType, SrcType>(model, model.accessors.at(primitive.attributes.at(attrib)));
}

struct GltfImageLoader {
    const std::filesystem::path baseDir;
    std::vector<ImageData> loadedImages;
    uint64_t bytesTotal{0};
};

bool loadImageFromGltf(
    tinygltf::Image* image,
    const int32_t imageIdx,
    std::string* err,
    std::string* warn,
    const int32_t /*reqWidth*/,
    const int32_t /*reqHeight*/,
    const uint8_t* bytes,
    const int32_t size,
    void* userPtr) {
    CRISP_CHECK(image != nullptr);
    logger->info("Loading image {:>4} '{}', byte size {} from {}.", imageIdx, image->name, size, image->uri);
    if (err && !err->empty()) {
        logger->error("Error while loading GLTF image: {}", *err);
    }
    if (warn && !warn->empty()) {
        logger->warn("Warning while loading GLTF image: {}", *warn);
    }

    GltfImageLoader& imageLoader{*static_cast<GltfImageLoader*>(userPtr)};
    imageLoader.bytesTotal += size;
    imageLoader.loadedImages.emplace_back(loadImage(std::span(bytes, size)).unwrap(), image->name);
    return true;
}

template <GlmAttrib GlmType, typename T>
GlmType toGlm(const std::vector<T>& values) {
    CRISP_CHECK_EQ(values.size(), GlmType::length());
    GlmType glmValue{};
    for (glm::length_t k = 0; k < GlmType::length(); ++k) {
        glmValue[k] = static_cast<typename GlmType::value_type>(values[k]);
    }
    return glmValue;
}

glm::vec3 getNodeTranslation(const tinygltf::Node& node) {
    return node.translation.empty() ? glm::vec3(0.0f) : toGlm<glm::vec3>(node.translation);
}

glm::quat getNodeRotation(const tinygltf::Node& node) {
    return node.rotation.empty() ? glm::quat(1.0f, 0.0f, 0.0f, 0.0f) : toGlm<glm::quat>(node.rotation);
}

glm::vec3 getNodeScale(const tinygltf::Node& node) {
    return node.scale.empty() ? glm::vec3(1.0f) : toGlm<glm::vec3>(node.scale);
}

glm::mat4 getNodeTransform(const tinygltf::Node& node) {
    glm::mat4 transform(1.0f);
    if (!node.matrix.empty()) {
        CRISP_CHECK_EQ(node.matrix.size(), 16);
        for (int32_t i = 0; i < 16; ++i) {
            transform[i / 4][i % 4] = static_cast<float>(node.matrix[i]);
        }
    }
    if (!node.translation.empty()) {
        transform = transform * glm::translate(toGlm<glm::vec3>(node.translation));
    }
    if (!node.rotation.empty()) {
        transform = transform * glm::toMat4(toGlm<glm::quat>(node.rotation));
    }
    if (!node.scale.empty()) {
        transform = transform * glm::scale(toGlm<glm::vec3>(node.scale));
    }
    return transform;
}

} // namespace

PbrMaterial createPbrMaterialFromGltfMaterial(
    const tinygltf::Model& model, const tinygltf::Material& material, GltfImageLoader& loader) {
    PbrMaterial pbrMaterial{};
    const auto getTexture = [&model, &loader, &pbrMaterial](const int32_t index, const int32_t textureIndex) {
        if (isValidGltfIndex(textureIndex)) {
            const int32_t sourceIndex = model.textures.at(textureIndex).source;
            pbrMaterial.textureKeys[index] = fmt::format("{}", sourceIndex);
            loader.loadedImages[sourceIndex].accessTypes[index] = true;
            return true;
        }
        return false;
    };

    getTexture(0, material.pbrMetallicRoughness.baseColorTexture.index);
    getTexture(1, material.normalTexture.index);
    if (getTexture(2, material.pbrMetallicRoughness.metallicRoughnessTexture.index)) {
        pbrMaterial.textureKeys[3] = pbrMaterial.textureKeys[2];
        loader.loadedImages[std::stoi(pbrMaterial.textureKeys[3])].accessTypes[3] = true;
    }
    getTexture(4, material.occlusionTexture.index);
    getTexture(5, material.emissiveTexture.index);

    pbrMaterial.params.albedo = toGlm<glm::vec4>(material.pbrMetallicRoughness.baseColorFactor);
    pbrMaterial.params.metallic = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
    pbrMaterial.params.roughness = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
    pbrMaterial.params.aoStrength = static_cast<float>(material.occlusionTexture.strength);

    return pbrMaterial;
}

TriangleMesh createMeshFromPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
    CRISP_CHECK_EQ(primitive.mode, TINYGLTF_MODE_TRIANGLES);

    if (!primitive.targets.empty()) {
        logger->info("Encountered morph targets in primitive will be skipped.");
    }

    std::vector<glm::vec3> positions{createBuffer<glm::vec3>(model, primitive, "POSITION").unwrap()};
    std::vector<glm::vec3> normals{createBuffer<glm::vec3>(model, primitive, "NORMAL").unwrap()};
    std::vector<glm::vec2> texCoords{createBuffer<glm::vec2>(model, primitive, "TEXCOORD_0").unwrap()};
    std::vector<glm::vec4> tangents{createBuffer<glm::vec4>(model, primitive, "TANGENT").unwrap()};

    CRISP_CHECK_GE_LT(primitive.indices, 0, static_cast<int32_t>(model.accessors.size()));
    std::vector<glm::uvec3> indices{loadIndexBuffer(model, model.accessors.at(primitive.indices)).unwrap()};

    TriangleMesh triangleMesh{std::move(positions), std::move(normals), std::move(texCoords), std::move(indices)};
    if (!tangents.empty()) {
        triangleMesh.setTangents(std::move(tangents));
    }

    return triangleMesh;
}

Result<std::vector<glm::mat4>> loadInverseBindTransforms(const tinygltf::Model& model, const uint32_t accessorIdx) {
    std::vector<glm::mat4> transforms{};
    const auto& accessor{model.accessors.at(accessorIdx)};
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        return resultError("InverseBindTransforms must be a float accessor");
    }

    const auto& bufferView{model.bufferViews.at(accessor.bufferView)};
    const auto& buffer{model.buffers.at(bufferView.buffer)};

    const size_t componentByteSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const size_t componentCount = tinygltf::GetNumComponentsInType(accessor.type);
    const size_t attributeByteSize{componentCount * componentByteSize};
    const size_t byteStride{bufferView.byteStride == 0 ? attributeByteSize : bufferView.byteStride};

    transforms.reserve(accessor.count);

    const size_t bufferRangeStart{bufferView.byteOffset + accessor.byteOffset};
    CRISP_CHECK(bufferRangeStart <= buffer.data.size());
    CRISP_CHECK(bufferRangeStart + accessor.count * byteStride <= buffer.data.size());

    glm::mat4 temp{};
    for (size_t i = 0; i < accessor.count; ++i) {
        const size_t offset{bufferRangeStart + i * byteStride};
        std::memcpy(&temp, buffer.data.data() + offset, attributeByteSize); // NOLINT
        transforms.emplace_back(temp);
    }
    return transforms;
}

SkinningData createSkinningData(const tinygltf::Model& model, const tinygltf::Skin& skin) {
    SkinningData skinningData{};
    const size_t jointCount{skin.joints.size()};
    FlatHashMap<int32_t, int32_t> modelNodeToLocalIdx(jointCount);
    Skeleton skeleton{};
    skeleton.setJointCount(jointCount);
    for (uint32_t i = 0; i < jointCount; ++i) {
        const int32_t jointNodeIdx{skin.joints.at(i)};
        modelNodeToLocalIdx[jointNodeIdx] = i; // NOLINT
        skeleton.joints[i].rotation = getNodeRotation(model.nodes.at(jointNodeIdx));
        skeleton.joints[i].translation = getNodeTranslation(model.nodes.at(jointNodeIdx));
        skeleton.joints[i].scale = getNodeScale(model.nodes.at(jointNodeIdx));
    }
    for (const auto& [modelNodeIdx, localIdx] : modelNodeToLocalIdx) {
        for (const auto& child : model.nodes.at(modelNodeIdx).children) {
            skeleton.parents[modelNodeToLocalIdx[child]] = localIdx;
        }
    }

    skinningData.inverseBindTransforms = loadInverseBindTransforms(model, skin.inverseBindMatrices).unwrap();
    skinningData.skeleton = std::move(skeleton);
    skinningData.modelNodeToLinearIdx = std::move(modelNodeToLocalIdx);
    return skinningData;
}

void createModelDataFromNode(
    const tinygltf::Model& model,
    const tinygltf::Node& node,
    GltfImageLoader& imageLoader,
    std::vector<ModelData>& models) {
    if (isValidGltfIndex(node.camera)) {
        logger->trace("Gltf contains camera information which will be unused.");
    }

    ModelData modelData{};
    if (isValidGltfIndex(node.skin)) {
        modelData.skinningData = createSkinningData(model, model.skins.at(node.skin));
    }

    if (isValidGltfIndex(node.mesh)) {
        const auto& mesh{model.meshes.at(node.mesh)};
        CRISP_CHECK(node.weights.empty(), "Morph targets are not supported!");

        for (const auto& primitive : mesh.primitives) {
            modelData.transform = getNodeTransform(node);
            modelData.mesh = createMeshFromPrimitive(model, primitive);

            modelData.mesh.setCustomAttribute(
                "weights0",
                createCustomVertexAttributeBuffer<glm::vec4>(
                    createBuffer<glm::vec4>(model, primitive, "WEIGHTS_0").unwrap()));
            modelData.mesh.setCustomAttribute(
                "indices0",
                createCustomVertexAttributeBuffer<glm::uvec4>(
                    createBuffer<glm::uvec4, glm::u16vec4>(model, primitive, "JOINTS_0").unwrap()));

            if (isValidGltfIndex(primitive.material)) {
                modelData.material =
                    createPbrMaterialFromGltfMaterial(model, model.materials.at(primitive.material), imageLoader);
            }

            models.push_back(std::move(modelData));
        }
    }

    for (const uint32_t childIdx : node.children) {
        createModelDataFromNode(model, model.nodes.at(childIdx), imageLoader, models);
    }
}

AnimationData createAnimationData(const tinygltf::Model& model, const tinygltf::Animation& animation) {
    AnimationData anim;
    for (const auto& ch : animation.channels) {
        AnimationChannel channel{};
        channel.targetNode = ch.target_node;
        channel.propertyName = ch.target_path;

        auto& sampler{animation.samplers.at(ch.sampler)};
        if (sampler.interpolation == "CUBICSPLINE") {
            channel.sampler.interpolation = AnimationSampler::Interpolation::CubicSpline;
        } else if (sampler.interpolation == "STEP") {
            channel.sampler.interpolation = AnimationSampler::Interpolation::Step;
        } else {
            channel.sampler.interpolation = AnimationSampler::Interpolation::Linear;
        }
        channel.sampler.inputs = createBuffer<float>(model, model.accessors.at(sampler.input)).unwrap();
        if (channel.propertyName == "translation") {
            auto vals = createBuffer<glm::vec3>(model, model.accessors.at(sampler.output)).unwrap();
            channel.sampler.outputs.resize(vals.size() * sizeof(glm::vec3));
            memcpy(channel.sampler.outputs.data(), vals.data(), vals.size() * sizeof(glm::vec3));
        } else if (channel.propertyName == "rotation") {
            auto vals = createBuffer<glm::quat>(model, model.accessors.at(sampler.output)).unwrap();
            channel.sampler.outputs.resize(vals.size() * sizeof(glm::quat));
            memcpy(channel.sampler.outputs.data(), vals.data(), vals.size() * sizeof(glm::quat));
        } else {
            auto vals = createBuffer<glm::vec3>(model, model.accessors.at(sampler.output)).unwrap();
            channel.sampler.outputs.resize(vals.size() * sizeof(glm::vec3));
            memcpy(channel.sampler.outputs.data(), vals.data(), vals.size() * sizeof(glm::vec3));
        }
        anim.channels.push_back(channel);
    }

    return anim;
}

PbrImageGroup createPbrImageData(
    std::string name, const std::span<ImageData> images, const std::span<ModelData> models) {
    PbrImageGroup imageData{.name = std::move(name)};
    std::vector<int32_t> remappedIndices(images.size(), -1);

    const auto appendImage = [&remappedIndices](std::vector<Image>& images, Image&& image, const size_t idx) {
        images.push_back(std::move(image));
        remappedIndices[idx] = static_cast<int32_t>(images.size()) - 1;
    };

    for (auto&& [idx, image] : std::views::enumerate(images)) {
        if (image.accessTypes[0]) {
            CRISP_CHECK(image.hasSingleAccessType());
            appendImage(imageData.albedoMaps, std::move(image.image), idx);
            continue;
        }
        if (image.accessTypes[1]) {
            CRISP_CHECK(image.hasSingleAccessType());
            appendImage(imageData.normalMaps, std::move(image.image), idx);
            continue;
        }
        if (image.accessTypes[2]) {
            appendImage(imageData.roughnessMaps, image.image.createFromChannel(kRoughnessMapChannel), idx);
        }
        if (image.accessTypes[3]) {
            appendImage(imageData.metallicMaps, image.image.createFromChannel(kMetallicMapChannel), idx);
        }
        if (image.accessTypes[4]) {
            CRISP_CHECK(image.hasSingleAccessType());
            appendImage(imageData.occlusionMaps, std::move(image.image), idx);
            continue;
        }
        if (image.accessTypes[5]) {
            CRISP_CHECK(image.hasSingleAccessType());
            appendImage(imageData.emissiveMaps, std::move(image.image), idx);
        }
    }

    PbrImageKeyCreator creator{imageData.name};
    for (auto& model : models) {
        for (auto&& [idx, index] : std::views::enumerate(model.material.textureKeys)) {
            if (!index.empty()) {
                index = creator.createMapKey(static_cast<uint32_t>(idx), remappedIndices[std::stoi(index)]);
            }
        }
    }

    return imageData;
}

Result<SceneData> loadGltfAsset(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return resultError("GLTF path '{}' doesn't exist!", path.string());
    }

    tinygltf::Model model;

    tinygltf::TinyGLTF loader;
    GltfImageLoader imageLoader{path.parent_path()};
    loader.SetImageLoader(loadImageFromGltf, &imageLoader);

    std::string err{};
    std::string warn{};
    const bool success{loader.LoadASCIIFromFile(&model, &err, &warn, path.string())};
    logger->trace("{} MB for images.", (imageLoader.bytesTotal >> 20) + 1);

    if (!warn.empty()) {
        logger->warn("GLTF warning from {}: {}", path.string(), warn);
    }
    if (!err.empty()) {
        return resultError("GLTF error from {}: {}", path.string(), err);
    }
    if (!success) {
        return resultError("Failed to parse GLTF: {}!", path.string());
    }
    if (model.nodes.empty()) {
        return resultError("Provided GLTF {} is empty!", path.string());
    }

    CRISP_CHECK_EQ(model.scenes.size(), 1, "Multi-scene GLTF is unsupported.");
    CRISP_CHECK_EQ(model.defaultScene, 0);
    const auto& scene{model.scenes.at(model.defaultScene)};

    SceneData sceneData{};
    for (const int32_t nodeIndex : scene.nodes) {
        createModelDataFromNode(model, model.nodes[nodeIndex], imageLoader, sceneData.models);
    }

    std::vector<AnimationData> animations{};
    for (const auto& anim : model.animations) {
        animations.push_back(createAnimationData(model, anim));

        // Check if animation matches the skeleton.
        for (auto& channel : animations.back().channels) {
            channel.targetNode = sceneData.models.back().skinningData.modelNodeToLinearIdx[channel.targetNode]; // NOLINT
        }
    }
    sceneData.models.back().animations = std::move(animations);

    sceneData.images = createPbrImageData(path.stem().string(), imageLoader.loadedImages, sceneData.models);

    return sceneData;
}

} // namespace crisp
