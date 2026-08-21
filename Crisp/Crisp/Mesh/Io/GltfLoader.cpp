#include <Crisp/Mesh/Io/GltfLoader.hpp>

#include <algorithm>
#include <chrono>
#include <limits>
#include <optional>
#include <ranges>
#include <string_view>

#pragma warning(push)
#pragma warning(disable : 4018) // Signed/unsigned comparison.
#pragma warning(disable : 4267) // Signed/unsigned comparison.
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>
#pragma warning(pop)

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/ThreadPool.hpp>
#include <Crisp/Image/Io/Utils.hpp>

namespace crisp {
namespace {
const auto logger = createLoggerMt("GltfLoader");

constexpr int32_t GltfInvalidIdx{-1};
constexpr uint32_t kMaximumImageDecodeThreadCount{8};

uint64_t hashImageContents(const std::span<const uint8_t> bytes) {
    const auto contents = std::string_view{reinterpret_cast<const char*>(bytes.data()), bytes.size()}; // NOLINT
    return ankerl::unordered_dense::hash<std::string_view>{}(contents);
}

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
struct IsGlmMat : public std::false_type {};

template <glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
struct IsGlmMat<glm::mat<C, R, T, Q>> : public std::true_type {};

template <typename T>
concept GlmVector = IsGlmVec<T>::value;

template <typename T>
concept GlmQuaternion = IsGlmQuat<T>::value;

template <typename T>
concept GlmMatrix = IsGlmMat<T>::value;

template <typename T>
concept GlmAttrib = GlmVector<T> || GlmQuaternion<T> || GlmMatrix<T>;

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

template <glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
struct ComponentTypeHelper<glm::mat<C, R, T, Q>> {
    using Type = T;
    static constexpr int32_t kCount = C * R;
};

template <GltfAttrib T>
using ComponentType = typename ComponentTypeHelper<T>::Type;

template <GltfAttrib T>
constexpr int32_t ComponentCount = ComponentTypeHelper<T>::kCount;

void validateAccessorRange(
    const tinygltf::BufferView& bufferView,
    const size_t bufferByteSize,
    const size_t accessorByteOffset,
    const size_t elementCount,
    const size_t byteStride,
    const size_t elementByteSize) {
    CRISP_CHECK_GT(elementByteSize, 0);
    CRISP_CHECK_GE(byteStride, elementByteSize);
    CRISP_CHECK_LE(bufferView.byteOffset, bufferByteSize);
    CRISP_CHECK_LE(bufferView.byteLength, bufferByteSize - bufferView.byteOffset);
    CRISP_CHECK_LE(accessorByteOffset, bufferView.byteLength);
    if (elementCount == 0) {
        return;
    }

    const size_t availableBytes = bufferView.byteLength - accessorByteOffset;
    CRISP_CHECK_LE(elementByteSize, availableBytes);
    CRISP_CHECK_LE(elementCount - 1, (availableBytes - elementByteSize) / byteStride);
}

struct AccessorBufferView {
    std::span<const uint8_t> bytes;
    size_t byteStride;
    size_t elementByteSize;
};

AccessorBufferView createAccessorBufferView(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
    CRISP_CHECK(isValidGltfIndex(accessor.bufferView), "Sparse GLTF accessors are unsupported.");
    CRISP_CHECK(!accessor.sparse.isSparse, "Sparse GLTF accessors are unsupported.");

    const auto& bufferView = model.bufferViews.at(accessor.bufferView);
    const auto& buffer = model.buffers.at(bufferView.buffer);
    const size_t componentByteSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const size_t componentCount = tinygltf::GetNumComponentsInType(accessor.type);
    const size_t elementByteSize = componentCount * componentByteSize;
    const size_t byteStride = bufferView.byteStride == 0 ? elementByteSize : bufferView.byteStride;
    validateAccessorRange(
        bufferView, buffer.data.size(), accessor.byteOffset, accessor.count, byteStride, elementByteSize);

    const size_t bufferRangeStart = bufferView.byteOffset + accessor.byteOffset;
    return {
        .bytes = std::span<const uint8_t>(buffer.data).subspan(bufferRangeStart),
        .byteStride = byteStride,
        .elementByteSize = elementByteSize,
    };
}

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
        []<bool flag = false>() { static_assert(flag, "Encountered unknown type in determineGltfComponentType()"); }();
    }
}

Result<std::vector<glm::uvec3>> loadIndexBuffer(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
    if (accessor.type != TINYGLTF_TYPE_SCALAR) {
        return resultError("Index accessor must contain scalar elements.");
    }
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE &&
        accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT &&
        accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        return resultError("Unsupported GLTF index component type {}.", accessor.componentType);
    }

    const size_t componentByteSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);

    CRISP_CHECK_EQ(accessor.count % glm::uvec3::length(), 0);
    const size_t triangleCount = accessor.count / glm::uvec3::length();

    const auto accessorView = createAccessorBufferView(model, accessor);
    CRISP_CHECK_EQ(accessorView.elementByteSize, componentByteSize);

    std::vector<glm::uvec3> indices(triangleCount);
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT && accessorView.byteStride == componentByteSize) {
        static_assert(sizeof(glm::uvec3) == 3 * sizeof(uint32_t));
        std::memcpy(indices.data(), accessorView.bytes.data(), accessor.count * componentByteSize); // NOLINT
    } else {
        for (size_t i = 0; i < accessor.count; ++i) {
            const size_t offset{i * accessorView.byteStride};
            uint32_t index{0};
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                index = accessorView.bytes[offset];
            } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                uint16_t index16;
                std::memcpy(&index16, accessorView.bytes.data() + offset, sizeof(index16)); // NOLINT
                index = index16;
            } else {
                std::memcpy(&index, accessorView.bytes.data() + offset, sizeof(index)); // NOLINT
            }
            indices[i / 3][i % 3] = index;
        }
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

    const auto accessorView = createAccessorBufferView(model, accessor);
    CRISP_CHECK_EQ(accessorView.elementByteSize, attributeByteSize);

    std::vector<DstType> attributes;
    attributes.reserve(accessor.count);

    if (static_cast<int32_t>(accessorView.byteStride) == attributeByteSize && std::is_same_v<DstType, SrcType> &&
        std::is_trivially_copy_assignable_v<DstType>) {
        attributes.resize(accessor.count);
        std::memcpy(attributes.data(), accessorView.bytes.data(), sizeof(DstType) * accessor.count); // NOLINT
        return attributes;
    }

    SrcType temp{};
    for (size_t i = 0; i < accessor.count; ++i) {
        const size_t offset{i * accessorView.byteStride};
        std::memcpy(&temp, accessorView.bytes.data() + offset, attributeByteSize); // NOLINT
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

template <GlmVector SrcType>
Result<std::vector<glm::vec4>> createNormalizedVec4Buffer(
    const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
    CRISP_TRY(auto values, createBuffer<SrcType>(model, accessor));
    std::vector<glm::vec4> normalizedValues;
    normalizedValues.reserve(values.size());
    constexpr auto maxValue = static_cast<float>(std::numeric_limits<ComponentType<SrcType>>::max());
    for (const auto& value : values) {
        normalizedValues.emplace_back(glm::vec4(value) / maxValue);
    }
    return normalizedValues;
}

Result<std::vector<glm::vec4>> loadWeightsBuffer(const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
    const auto& accessor = model.accessors.at(primitive.attributes.at("WEIGHTS_0"));
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
        return createBuffer<glm::vec4>(model, accessor);
    }
    if (!accessor.normalized) {
        return resultError("Integer GLTF weights must use normalized accessors.");
    }
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        return createNormalizedVec4Buffer<glm::u8vec4>(model, accessor);
    }
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        return createNormalizedVec4Buffer<glm::u16vec4>(model, accessor);
    }
    return resultError("Unsupported GLTF weight component type {}.", accessor.componentType);
}

Result<std::vector<glm::uvec4>> loadJointIndexBuffer(
    const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
    const auto& accessor = model.accessors.at(primitive.attributes.at("JOINTS_0"));
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        return createBuffer<glm::uvec4, glm::u8vec4>(model, accessor);
    }
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        return createBuffer<glm::uvec4, glm::u16vec4>(model, accessor);
    }
    return resultError("Unsupported GLTF joint-index component type {}.", accessor.componentType);
}

struct EncodedGltfImage {
    std::vector<uint8_t> bytes;
    std::string name;
};

struct GltfImageLoader {
    std::vector<EncodedGltfImage> encodedImages;
    std::vector<ImageData> loadedImages;
    std::vector<uint32_t> sourceToLoadedImage;
    std::vector<Image> ormImages;
    FlatHashMap<uint64_t, uint32_t> ormImageIndices;
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
    if (image == nullptr || userPtr == nullptr || imageIdx < 0 || bytes == nullptr || size <= 0) {
        if (err != nullptr) {
            *err = fmt::format("Invalid encoded GLTF image at index {}.", imageIdx);
        }
        return false;
    }

    CRISP_LOGT("Read image {:>4} '{}', byte size {} from {}.", imageIdx, image->name, size, image->uri);
    if (err && !err->empty()) {
        CRISP_LOGE("Error while loading GLTF image: {}", *err);
    }
    if (warn && !warn->empty()) {
        CRISP_LOGW("Warning while loading GLTF image: {}", *warn);
    }

    GltfImageLoader& imageLoader{*static_cast<GltfImageLoader*>(userPtr)};
    const auto imageIndex = static_cast<size_t>(imageIdx);
    if (imageLoader.encodedImages.size() <= imageIndex) {
        imageLoader.encodedImages.resize(imageIndex + 1);
    }

    auto& encodedImage = imageLoader.encodedImages[imageIndex];
    encodedImage.bytes.assign(bytes, bytes + size); // NOLINT
    encodedImage.name = image->name;
    imageLoader.bytesTotal += static_cast<uint64_t>(size);
    return true;
}

Result<std::vector<ImageData>> decodeGltfImages(GltfImageLoader& imageLoader) {
    if (imageLoader.encodedImages.empty()) {
        return std::vector<ImageData>{};
    }

    std::vector<size_t> uniqueImageIndices;
    uniqueImageIndices.reserve(imageLoader.encodedImages.size());
    imageLoader.sourceToLoadedImage.resize(imageLoader.encodedImages.size());

    // Each hash owns a small candidate bucket so hash collisions can be resolved with exact byte comparisons.
    FlatHashMap<uint64_t, std::vector<size_t>> imageIndicesByHash;
    uint64_t uniqueBytesTotal{0};
    const auto deduplicationStart = std::chrono::steady_clock::now();
    for (size_t imageIdx = 0; imageIdx < imageLoader.encodedImages.size(); ++imageIdx) {
        const auto& encodedImage = imageLoader.encodedImages[imageIdx];
        auto& matchingHashIndices = imageIndicesByHash[hashImageContents(encodedImage.bytes)];

        const auto duplicate = std::ranges::find_if(matchingHashIndices, [&](const size_t candidateIdx) {
            return imageLoader.encodedImages[candidateIdx].bytes == encodedImage.bytes;
        });
        if (duplicate != matchingHashIndices.end()) {
            imageLoader.sourceToLoadedImage[imageIdx] = imageLoader.sourceToLoadedImage[*duplicate];
            continue;
        }

        imageLoader.sourceToLoadedImage[imageIdx] = static_cast<uint32_t>(uniqueImageIndices.size());
        uniqueImageIndices.push_back(imageIdx);
        matchingHashIndices.push_back(imageIdx);
        uniqueBytesTotal += encodedImage.bytes.size();
    }
    const std::chrono::duration<double, std::milli> deduplicationDuration{
        std::chrono::steady_clock::now() - deduplicationStart};

    const uint32_t hardwareThreadCount{std::max(1u, std::thread::hardware_concurrency())};
    const uint32_t decodeThreadCount{std::min(
        {hardwareThreadCount, kMaximumImageDecodeThreadCount, static_cast<uint32_t>(uniqueImageIndices.size())})};

    std::vector<std::optional<Image>> decodedImages(uniqueImageIndices.size());
    std::vector<std::string> decodeErrors(uniqueImageIndices.size());

    const auto decodeStart = std::chrono::steady_clock::now();
    ThreadPool threadPool(decodeThreadCount);
    threadPool.parallelFor(uniqueImageIndices.size(), [&](const size_t uniqueIdx, const size_t /*threadIdx*/) {
        const size_t imageIdx = uniqueImageIndices[uniqueIdx];
        const auto& encodedImage = imageLoader.encodedImages[imageIdx];
        if (encodedImage.bytes.empty()) {
            decodeErrors[uniqueIdx] = "No encoded image data was provided.";
            return;
        }

        auto decodedImage = loadImage(std::span<const uint8_t>(encodedImage.bytes));
        if (!decodedImage) {
            decodeErrors[uniqueIdx] = std::move(decodedImage).getError();
            return;
        }
        decodedImages[uniqueIdx] = std::move(decodedImage).extract();
    });

    std::vector<ImageData> images;
    images.reserve(decodedImages.size());
    for (size_t uniqueIdx = 0; uniqueIdx < decodedImages.size(); ++uniqueIdx) {
        const size_t imageIdx = uniqueImageIndices[uniqueIdx];
        if (!decodeErrors[uniqueIdx].empty()) {
            return resultError(
                "Failed to decode GLTF image {} '{}': {}",
                imageIdx,
                imageLoader.encodedImages[imageIdx].name,
                decodeErrors[uniqueIdx]);
        }
        if (!decodedImages[uniqueIdx]) {
            return resultError(
                "GLTF image {} '{}' produced no decoded data.", imageIdx, imageLoader.encodedImages[imageIdx].name);
        }
        images.emplace_back(std::move(*decodedImages[uniqueIdx]), imageLoader.encodedImages[imageIdx].name);
    }

    const std::chrono::duration<double, std::milli> decodeDuration{std::chrono::steady_clock::now() - decodeStart};
    CRISP_LOGI(
        "Decoded {} unique GLTF images ({:.1f} MiB encoded) on {} threads in {:.1f} ms; skipped {} duplicates "
        "({:.1f} MiB).",
        images.size(),
        static_cast<double>(uniqueBytesTotal) / (1024.0 * 1024.0),
        decodeThreadCount,
        decodeDuration.count(),
        imageLoader.encodedImages.size() - images.size(),
        static_cast<double>(imageLoader.bytesTotal - uniqueBytesTotal) / (1024.0 * 1024.0));
    CRISP_LOGI("Content-hash image deduplication took {:.1f} ms.", deduplicationDuration.count());

    return images;
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
    const auto getImageIndex = [&model, &loader](const int32_t textureIndex) -> std::optional<uint32_t> {
        if (!isValidGltfIndex(textureIndex)) {
            return std::nullopt;
        }
        const int32_t sourceIndex = model.textures.at(textureIndex).source;
        CRISP_CHECK(isValidGltfIndex(sourceIndex));
        return loader.sourceToLoadedImage.at(sourceIndex);
    };
    const auto setTexture = [&loader, &pbrMaterial, &getImageIndex](const uint32_t mapIndex, const int32_t textureIndex) {
        if (const auto imageIndex = getImageIndex(textureIndex)) {
            pbrMaterial.textureKeys[mapIndex] = fmt::format("{}", *imageIndex);
            loader.loadedImages[*imageIndex].accessTypes[mapIndex] = true;
        }
    };

    setTexture(kPbrAlbedoMapIndex, material.pbrMetallicRoughness.baseColorTexture.index);
    setTexture(kPbrNormalMapIndex, material.normalTexture.index);
    setTexture(kPbrEmissiveMapIndex, material.emissiveTexture.index);

    const auto metallicRoughnessImage = getImageIndex(material.pbrMetallicRoughness.metallicRoughnessTexture.index);
    const auto occlusionImage = getImageIndex(material.occlusionTexture.index);
    if (metallicRoughnessImage || occlusionImage) {
        constexpr uint32_t kNoImage{std::numeric_limits<uint32_t>::max()};
        const uint32_t metallicRoughnessIndex = metallicRoughnessImage.value_or(kNoImage);
        const uint32_t occlusionIndex = occlusionImage.value_or(kNoImage);
        const uint64_t ormSourceKey = (static_cast<uint64_t>(occlusionIndex) << 32) | metallicRoughnessIndex;

        auto ormImage = loader.ormImageIndices.find(ormSourceKey);
        if (ormImage == loader.ormImageIndices.end()) {
            const Image* metallicRoughness =
                metallicRoughnessImage ? &loader.loadedImages[*metallicRoughnessImage].image : nullptr;
            const Image* occlusion = occlusionImage ? &loader.loadedImages[*occlusionImage].image : nullptr;
            const uint32_t ormImageIndex = static_cast<uint32_t>(loader.ormImages.size());
            if (metallicRoughnessImage == occlusionImage) {
                loader.ormImages.push_back(*metallicRoughness);
            } else {
                loader.ormImages.push_back(createPbrOrmMap({
                    .occlusion = occlusion,
                    .occlusionChannel = 0,
                    .roughness = metallicRoughness,
                    .roughnessChannel = 1,
                    .metallic = metallicRoughness,
                    .metallicChannel = 2,
                }));
            }
            ormImage = loader.ormImageIndices.emplace(ormSourceKey, ormImageIndex).first;
        }
        pbrMaterial.textureKeys[kPbrOrmMapIndex] = fmt::format("{}", ormImage->second);
    }

    pbrMaterial.params.albedo = toGlm<glm::vec4>(material.pbrMetallicRoughness.baseColorFactor);
    pbrMaterial.params.metallic = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
    pbrMaterial.params.roughness = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
    pbrMaterial.params.aoStrength = static_cast<float>(material.occlusionTexture.strength);

    return pbrMaterial;
}

TriangleMesh createMeshFromPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
    CRISP_CHECK_EQ(primitive.mode, TINYGLTF_MODE_TRIANGLES);

    if (!primitive.targets.empty()) {
        CRISP_LOGI("Encountered morph targets in primitive will be skipped.");
    }

    std::vector<glm::vec3> positions{createBuffer<glm::vec3>(model, primitive, "POSITION").unwrap()};
    std::vector<glm::vec3> normals{createBuffer<glm::vec3>(model, primitive, "NORMAL").unwrap()};
    std::vector<glm::vec2> texCoords{createBuffer<glm::vec2>(model, primitive, "TEXCOORD_0").unwrap()};
    std::vector<glm::vec4> tangents{createBuffer<glm::vec4>(model, primitive, "TANGENT").unwrap()};

    std::vector<glm::uvec3> indices;
    if (isValidGltfIndex(primitive.indices)) {
        CRISP_CHECK_LT(primitive.indices, static_cast<int32_t>(model.accessors.size()));
        indices = loadIndexBuffer(model, model.accessors.at(primitive.indices)).unwrap();
    } else {
        CRISP_CHECK_EQ(positions.size() % 3, 0, "Non-indexed triangle primitives require a multiple of 3 vertices.");
        CRISP_CHECK_LE(positions.size(), std::numeric_limits<uint32_t>::max());
        indices.reserve(positions.size() / 3);
        for (size_t vertex = 0; vertex < positions.size(); vertex += 3) {
            indices.emplace_back(
                static_cast<uint32_t>(vertex), static_cast<uint32_t>(vertex + 1), static_cast<uint32_t>(vertex + 2));
        }
    }

    TriangleMesh triangleMesh{std::move(positions), std::move(normals), std::move(texCoords), std::move(indices)};
    if (!tangents.empty()) {
        triangleMesh.setTangents(std::move(tangents));
    }

    return triangleMesh;
}

Result<std::vector<glm::mat4>> loadInverseBindTransforms(const tinygltf::Model& model, const uint32_t accessorIdx) {
    return createBuffer<glm::mat4>(model, model.accessors.at(accessorIdx));
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
            const auto childJoint = modelNodeToLocalIdx.find(child);
            if (childJoint != modelNodeToLocalIdx.end()) {
                skeleton.parents[childJoint->second] = localIdx;
            }
        }
    }

    if (isValidGltfIndex(skin.inverseBindMatrices)) {
        skinningData.inverseBindTransforms = loadInverseBindTransforms(model, skin.inverseBindMatrices).unwrap();
        CRISP_CHECK_EQ(skinningData.inverseBindTransforms.size(), jointCount);
    } else {
        skinningData.inverseBindTransforms.resize(jointCount, glm::mat4(1.0f));
    }
    skinningData.skeleton = std::move(skeleton);
    skinningData.modelNodeToLinearIdx = std::move(modelNodeToLocalIdx);
    return skinningData;
}

void createModelDataFromNode(
    const tinygltf::Model& model,
    const tinygltf::Node& node,
    const glm::mat4& parentTransform,
    GltfImageLoader& imageLoader,
    std::vector<ModelData>& models) {
    if (isValidGltfIndex(node.camera)) {
        CRISP_LOGT("Gltf contains camera information which will be unused.");
    }

    std::optional<SkinningData> skinningData;
    if (isValidGltfIndex(node.skin)) {
        skinningData = createSkinningData(model, model.skins.at(node.skin));
    }

    const glm::mat4 worldTransform = parentTransform * getNodeTransform(node);

    if (isValidGltfIndex(node.mesh)) {
        const auto& mesh{model.meshes.at(node.mesh)};
        CRISP_CHECK(node.weights.empty(), "Morph targets are not supported!");

        for (const auto& primitive : mesh.primitives) {
            ModelData modelData{};
            modelData.transform = worldTransform;
            if (skinningData) {
                modelData.skinningData = *skinningData;
            }
            modelData.mesh = createMeshFromPrimitive(model, primitive);

            const bool hasWeights = primitive.attributes.contains("WEIGHTS_0");
            const bool hasJoints = primitive.attributes.contains("JOINTS_0");
            CRISP_CHECK_EQ(hasWeights, hasJoints, "GLTF skinning requires both WEIGHTS_0 and JOINTS_0.");
            if (hasWeights) {
                auto weights = loadWeightsBuffer(model, primitive).unwrap();
                auto joints = loadJointIndexBuffer(model, primitive).unwrap();
                CRISP_CHECK_EQ(weights.size(), modelData.mesh.getVertexCount());
                CRISP_CHECK_EQ(joints.size(), modelData.mesh.getVertexCount());
                modelData.mesh.setCustomAttribute("weights0", createCustomVertexAttributeBuffer<glm::vec4>(weights));
                modelData.mesh.setCustomAttribute("indices0", createCustomVertexAttributeBuffer<glm::uvec4>(joints));
            }

            if (isValidGltfIndex(primitive.material)) {
                modelData.material =
                    createPbrMaterialFromGltfMaterial(model, model.materials.at(primitive.material), imageLoader);
            }

            models.push_back(std::move(modelData));
        }
    }

    for (const uint32_t childIdx : node.children) {
        createModelDataFromNode(model, model.nodes.at(childIdx), worldTransform, imageLoader, models);
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
    std::string name, const std::span<ImageData> images, std::vector<Image> ormImages, const std::span<ModelData> models) {
    PbrImageGroup imageData{.name = std::move(name), .ormMaps = std::move(ormImages)};
    std::array<std::vector<int32_t>, kPbrMapTypeCount> remappedIndices;
    for (auto& indices : remappedIndices) {
        indices.resize(images.size(), -1);
    }

    const auto appendImage =
        [&remappedIndices](std::vector<Image>& images, Image image, const size_t imageIdx, const size_t typeIdx) {
            images.push_back(std::move(image));
            remappedIndices[typeIdx][imageIdx] = static_cast<int32_t>(images.size()) - 1;
        };

    for (auto&& [idx, image] : std::views::enumerate(images)) {
        const std::array<std::pair<uint32_t, std::vector<Image>*>, 3> fullImageMaps{{
            {0, &imageData.albedoMaps},
            {1, &imageData.normalMaps},
            {3, &imageData.emissiveMaps},
        }};
        size_t remainingFullImageUses = std::ranges::count_if(fullImageMaps, [&image](const auto& map) {
            return image.accessTypes[map.first];
        });
        for (const auto& [typeIdx, maps] : fullImageMaps) {
            if (!image.accessTypes[typeIdx]) {
                continue;
            }
            --remainingFullImageUses;
            appendImage(*maps, remainingFullImageUses == 0 ? std::move(image.image) : image.image, idx, typeIdx);
        }
    }

    PbrImageKeyCreator creator{imageData.name};
    for (auto& model : models) {
        for (auto&& [idx, index] : std::views::enumerate(model.material.textureKeys)) {
            if (!index.empty()) {
                const int32_t remappedIndex =
                    idx == kPbrOrmMapIndex ? std::stoi(index) : remappedIndices[idx][std::stoi(index)];
                index = creator.createMapKey(static_cast<uint32_t>(idx), remappedIndex);
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
    GltfImageLoader imageLoader{};
    loader.SetImageLoader(loadImageFromGltf, &imageLoader);

    std::string err{};
    std::string warn{};
    const auto parseStart = std::chrono::steady_clock::now();
    const bool success =
        path.extension() == ".glb"
            ? loader.LoadBinaryFromFile(&model, &err, &warn, path.string())
            : loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
    const std::chrono::duration<double, std::milli> parseDuration{std::chrono::steady_clock::now() - parseStart};

    if (!warn.empty()) {
        CRISP_LOGW("GLTF warning from {}: {}", path.string(), warn);
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

    CRISP_LOGI(
        "Parsed GLTF and read {} encoded images in {:.1f} ms.", imageLoader.encodedImages.size(), parseDuration.count());
    CRISP_TRY(imageLoader.loadedImages, decodeGltfImages(imageLoader));

    SceneData sceneData{};
    for (const int32_t nodeIndex : scene.nodes) {
        createModelDataFromNode(model, model.nodes[nodeIndex], glm::mat4(1.0f), imageLoader, sceneData.models);
    }

    std::vector<AnimationData> animations{};
    animations.reserve(model.animations.size());
    for (const auto& animation : model.animations) {
        animations.push_back(createAnimationData(model, animation));
    }

    for (auto& sceneModel : sceneData.models) {
        const auto& jointIndices = sceneModel.skinningData.modelNodeToLinearIdx;
        if (jointIndices.empty()) {
            continue;
        }

        for (const auto& animation : animations) {
            AnimationData filteredAnimation{};
            filteredAnimation.channels.reserve(animation.channels.size());
            for (const auto& channel : animation.channels) {
                const auto joint = jointIndices.find(static_cast<int32_t>(channel.targetNode));
                if (joint == jointIndices.end()) {
                    continue;
                }

                auto remappedChannel = channel;
                remappedChannel.targetNode = static_cast<uint32_t>(joint->second);
                filteredAnimation.channels.push_back(std::move(remappedChannel));
            }

            if (!filteredAnimation.channels.empty()) {
                sceneModel.animations.push_back(std::move(filteredAnimation));
            }
        }
    }

    sceneData.images = createPbrImageData(
        path.stem().string(), imageLoader.loadedImages, std::move(imageLoader.ormImages), sceneData.models);

    return sceneData;
}

} // namespace crisp
