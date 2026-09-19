#include <Crisp/Mesh/Io/GltfLoader.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <optional>
#include <ranges>
#include <string_view>

#pragma warning(push)
#pragma warning(disable : 4018) // Signed/unsigned comparison.
#pragma warning(disable : 4267) // Conversion, possible loss of data.
#pragma warning(disable : 4244) // Conversion, possible loss of data.
#pragma warning(disable : 4996) // Deprecated CRT functions.
#pragma warning(disable : 4505) // Unreferenced function with internal linkage removed.
#define TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_ENABLE_FS
#include <tiny_gltf_v3.h>
#pragma warning(pop)

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/ThreadPool.hpp>
#include <Crisp/Image/Io/Utils.hpp>
#include <Crisp/Io/FileUtils.hpp>

namespace crisp {
namespace {
CRISP_MAKE_LOGGER_MT("GltfLoader");

constexpr uint32_t kBaselineImageDecodeThreadCount{8};
constexpr uint32_t kTargetImagesPerDecodeThread{4};

uint64_t hashImageContents(const std::span<const uint8_t> bytes) {
    const auto contents = std::string_view{reinterpret_cast<const char*>(bytes.data()), bytes.size()}; // NOLINT
    return ankerl::unordered_dense::hash<std::string_view>{}(contents);
}

constexpr bool isValidGltfIndex(const int32_t index) {
    return index != TG3_INDEX_NONE;
}

std::string_view toStringView(const tg3_str& str) {
    return str.data == nullptr ? std::string_view{} : std::string_view{str.data, str.len};
}

bool equalsCStr(const tg3_str& str, const char* value) {
    return tg3_str_equals_cstr(str, value) != 0;
}

template <typename T>
std::span<const T> toSpan(const T* data, const uint32_t count) {
    return data == nullptr ? std::span<const T>{} : std::span<const T>{data, count};
}

int32_t findAttributeAccessor(const tg3_primitive& primitive, const char* name) {
    for (const auto& attribute : toSpan(primitive.attributes, primitive.attributes_count)) {
        if (equalsCStr(attribute.key, name)) {
            return attribute.value;
        }
    }
    return TG3_INDEX_NONE;
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
    const tg3_buffer_view& bufferView,
    const size_t bufferByteSize,
    const size_t accessorByteOffset,
    const size_t elementCount,
    const size_t byteStride,
    const size_t elementByteSize) {
    CRISP_CHECK_GT(elementByteSize, 0);
    CRISP_CHECK_GE(byteStride, elementByteSize);
    CRISP_CHECK_LE(bufferView.byte_offset, bufferByteSize);
    CRISP_CHECK_LE(bufferView.byte_length, bufferByteSize - bufferView.byte_offset);
    CRISP_CHECK_LE(accessorByteOffset, bufferView.byte_length);
    if (elementCount == 0) {
        return;
    }

    const size_t availableBytes = bufferView.byte_length - accessorByteOffset;
    CRISP_CHECK_LE(elementByteSize, availableBytes);
    CRISP_CHECK_LE(elementCount - 1, (availableBytes - elementByteSize) / byteStride);
}

struct AccessorBufferView {
    std::span<const uint8_t> bytes;
    size_t byteStride;
    size_t elementByteSize;
};

struct MaterialWarningCounts {
    uint32_t blendMaterialCount{0};
    uint32_t unsupportedTexCoordMaterialCount{0};
    uint32_t customSamplerMaterialCount{0};
};

AccessorBufferView createAccessorBufferView(const tg3_model& model, const tg3_accessor& accessor) {
    CRISP_CHECK(isValidGltfIndex(accessor.buffer_view), "Sparse GLTF accessors are unsupported.");
    CRISP_CHECK(accessor.sparse.is_sparse == 0, "Sparse GLTF accessors are unsupported.");

    const auto bufferViews = toSpan(model.buffer_views, model.buffer_views_count);
    CRISP_CHECK_INDEX(accessor.buffer_view, bufferViews);
    const auto& bufferView = bufferViews[accessor.buffer_view];

    const auto buffers = toSpan(model.buffers, model.buffers_count);
    CRISP_CHECK_INDEX(bufferView.buffer, buffers);
    const auto& buffer = buffers[bufferView.buffer];

    const size_t componentByteSize = tg3_component_size(accessor.component_type);
    const size_t componentCount = tg3_num_components(accessor.type);
    const size_t elementByteSize = componentCount * componentByteSize;
    const size_t byteStride = bufferView.byte_stride == 0 ? elementByteSize : bufferView.byte_stride;
    validateAccessorRange(
        bufferView, buffer.data.count, accessor.byte_offset, accessor.count, byteStride, elementByteSize);

    const size_t bufferRangeStart = bufferView.byte_offset + accessor.byte_offset;
    return {
        .bytes = toSpan(buffer.data.data, static_cast<uint32_t>(buffer.data.count)).subspan(bufferRangeStart),
        .byteStride = byteStride,
        .elementByteSize = elementByteSize,
    };
}

template <ScalarAttrib T>
int32_t determineGltfComponentType() {
    if constexpr (std::is_same_v<T, float>) {
        return TG3_COMPONENT_TYPE_FLOAT;
    } else if constexpr (std::is_same_v<T, double>) {
        return TG3_COMPONENT_TYPE_DOUBLE;
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        return TG3_COMPONENT_TYPE_UNSIGNED_INT;
    } else if constexpr (std::is_same_v<T, uint16_t>) {
        return TG3_COMPONENT_TYPE_UNSIGNED_SHORT;
    } else if constexpr (std::is_same_v<T, uint8_t>) {
        return TG3_COMPONENT_TYPE_UNSIGNED_BYTE;
    } else if constexpr (std::is_same_v<T, int32_t>) {
        return TG3_COMPONENT_TYPE_INT;
    } else if constexpr (std::is_same_v<T, int16_t>) {
        return TG3_COMPONENT_TYPE_SHORT;
    } else if constexpr (std::is_same_v<T, int8_t>) {
        return TG3_COMPONENT_TYPE_BYTE;
    } else {
        []<bool flag = false>() { static_assert(flag, "Encountered unknown type in determineGltfComponentType()"); }();
    }
}

Result<std::vector<glm::uvec3>> loadIndexBuffer(const tg3_model& model, const tg3_accessor& accessor) {
    if (accessor.type != TG3_TYPE_SCALAR) {
        return resultError("Index accessor must contain scalar elements.");
    }
    if (accessor.component_type != TG3_COMPONENT_TYPE_UNSIGNED_BYTE &&
        accessor.component_type != TG3_COMPONENT_TYPE_UNSIGNED_SHORT &&
        accessor.component_type != TG3_COMPONENT_TYPE_UNSIGNED_INT) {
        return resultError("Unsupported GLTF index component type {}.", accessor.component_type);
    }

    const size_t componentByteSize = tg3_component_size(accessor.component_type);
    CRISP_CHECK_EQ(accessor.count % glm::uvec3::length(), 0);
    const size_t triangleCount = accessor.count / glm::uvec3::length();

    const auto accessorView = createAccessorBufferView(model, accessor);
    CRISP_CHECK_EQ(accessorView.elementByteSize, componentByteSize);

    std::vector<glm::uvec3> indices(triangleCount);
    if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_INT &&
        accessorView.byteStride == componentByteSize) {
        static_assert(sizeof(glm::uvec3) == 3 * sizeof(uint32_t));
        std::memcpy(indices.data(), accessorView.bytes.data(), accessor.count * componentByteSize); // NOLINT
    } else {
        for (size_t i = 0; i < accessor.count; ++i) {
            const size_t offset{i * accessorView.byteStride};
            uint32_t index{0};
            if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_BYTE) {
                index = accessorView.bytes[offset];
            } else if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) {
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
Result<std::vector<DstType>> createBuffer(const tg3_model& model, const tg3_accessor& accessor) {
    const int32_t componentByteSize = tg3_component_size(accessor.component_type);
    CRISP_CHECK_EQ(accessor.component_type, determineGltfComponentType<ComponentType<SrcType>>());

    const int32_t componentCount = tg3_num_components(accessor.type);
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
    const tg3_model& model, const tg3_primitive& primitive, const char* attrib) {
    const int32_t accessorIdx = findAttributeAccessor(primitive, attrib);
    if (!isValidGltfIndex(accessorIdx)) {
        return std::vector<DstType>{};
    }
    return createBuffer<DstType, SrcType>(model, model.accessors[accessorIdx]);
}

template <GlmVector SrcType>
Result<std::vector<glm::vec4>> createNormalizedVec4Buffer(const tg3_model& model, const tg3_accessor& accessor) {
    CRISP_TRY(auto values, createBuffer<SrcType>(model, accessor));
    std::vector<glm::vec4> normalizedValues;
    normalizedValues.reserve(values.size());
    constexpr auto maxValue = static_cast<float>(std::numeric_limits<ComponentType<SrcType>>::max());
    for (const auto& value : values) {
        normalizedValues.emplace_back(glm::vec4(value) / maxValue);
    }
    return normalizedValues;
}

Result<std::vector<glm::vec4>> loadWeightsBuffer(const tg3_model& model, const tg3_primitive& primitive) {
    const auto& accessor = model.accessors[findAttributeAccessor(primitive, "WEIGHTS_0")];
    if (accessor.component_type == TG3_COMPONENT_TYPE_FLOAT) {
        return createBuffer<glm::vec4>(model, accessor);
    }
    if (accessor.normalized == 0) {
        return resultError("Integer GLTF weights must use normalized accessors.");
    }
    if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_BYTE) {
        return createNormalizedVec4Buffer<glm::u8vec4>(model, accessor);
    }
    if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) {
        return createNormalizedVec4Buffer<glm::u16vec4>(model, accessor);
    }
    return resultError("Unsupported GLTF weight component type {}.", accessor.component_type);
}

Result<std::vector<glm::uvec4>> loadJointIndexBuffer(const tg3_model& model, const tg3_primitive& primitive) {
    const auto& accessor = model.accessors[findAttributeAccessor(primitive, "JOINTS_0")];
    if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_BYTE) {
        return createBuffer<glm::uvec4, glm::u8vec4>(model, accessor);
    }
    if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) {
        return createBuffer<glm::uvec4, glm::u16vec4>(model, accessor);
    }
    return resultError("Unsupported GLTF joint-index component type {}.", accessor.component_type);
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
};

int8_t base64SymbolValue(const char symbol) {
    if (symbol >= 'A' && symbol <= 'Z') {
        return static_cast<int8_t>(symbol - 'A');
    }
    if (symbol >= 'a' && symbol <= 'z') {
        return static_cast<int8_t>(symbol - 'a' + 26);
    }
    if (symbol >= '0' && symbol <= '9') {
        return static_cast<int8_t>(symbol - '0' + 52);
    }
    if (symbol == '+' || symbol == '-') {
        return 62;
    }
    if (symbol == '/' || symbol == '_') {
        return 63;
    }
    return -1;
}

Result<std::vector<uint8_t>> decodeBase64(const std::string_view encoded) {
    std::vector<uint8_t> decoded;
    decoded.reserve(encoded.size() / 4 * 3);

    uint32_t accumulator{0};
    uint32_t bitCount{0};
    for (const char symbol : encoded) {
        if (symbol == '=') {
            break;
        }
        if (symbol == '\r' || symbol == '\n' || symbol == ' ' || symbol == '\t') {
            continue;
        }

        const int8_t value = base64SymbolValue(symbol);
        if (value < 0) {
            return resultError("Encountered invalid base64 symbol '{}'.", symbol);
        }

        accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
        bitCount += 6;
        if (bitCount >= 8) {
            bitCount -= 8;
            decoded.push_back(static_cast<uint8_t>((accumulator >> bitCount) & 0xFFu));
        }
    }

    return decoded;
}

// Only the escapes that show up in glTF URIs; the parser hands us the raw, still-escaped string.
std::string decodePercentEncoding(const std::string_view uri) {
    std::string decoded;
    decoded.reserve(uri.size());
    for (size_t i = 0; i < uri.size(); ++i) {
        if (uri[i] != '%' || i + 2 >= uri.size()) {
            decoded.push_back(uri[i]);
            continue;
        }

        const auto hexValue = [](const char symbol) -> int32_t {
            if (symbol >= '0' && symbol <= '9') {
                return symbol - '0';
            }
            if (symbol >= 'a' && symbol <= 'f') {
                return symbol - 'a' + 10;
            }
            if (symbol >= 'A' && symbol <= 'F') {
                return symbol - 'A' + 10;
            }
            return -1;
        };

        const int32_t high = hexValue(uri[i + 1]);
        const int32_t low = hexValue(uri[i + 2]);
        if (high < 0 || low < 0) {
            decoded.push_back(uri[i]);
            continue;
        }

        decoded.push_back(static_cast<char>((high << 4) | low));
        i += 2;
    }
    return decoded;
}

// tinygltf v3 parses image metadata only, so the bytes behind each source have to be resolved here.
Result<std::vector<uint8_t>> resolveImageBytes(
    const tg3_model& model, const tg3_image& image, const std::filesystem::path& baseDir) {
    if (isValidGltfIndex(image.buffer_view)) {
        const auto bufferViews = toSpan(model.buffer_views, model.buffer_views_count);
        CRISP_CHECK_INDEX(image.buffer_view, bufferViews);
        const auto& bufferView = bufferViews[image.buffer_view];

        const auto buffers = toSpan(model.buffers, model.buffers_count);
        CRISP_CHECK_INDEX(bufferView.buffer, buffers);
        const auto& buffer = buffers[bufferView.buffer];

        if (bufferView.byte_offset + bufferView.byte_length > buffer.data.count) {
            return resultError("GLTF image buffer view exceeds its buffer.");
        }

        const auto* begin = buffer.data.data + bufferView.byte_offset; // NOLINT
        return std::vector<uint8_t>(begin, begin + bufferView.byte_length); // NOLINT
    }

    const auto uri = toStringView(image.uri);
    if (uri.empty()) {
        return resultError("GLTF image has neither a buffer view nor a URI.");
    }

    if (tg3_is_data_uri(uri.data(), static_cast<uint32_t>(uri.size())) != 0) {
        const size_t commaPos = uri.find(',');
        if (commaPos == std::string_view::npos) {
            return resultError("GLTF image data URI has no payload separator.");
        }
        if (uri.find(";base64", 0) >= commaPos) {
            return resultError("Only base64-encoded GLTF image data URIs are supported.");
        }
        return decodeBase64(uri.substr(commaPos + 1));
    }

    const auto imagePath = baseDir / decodePercentEncoding(uri);
    CRISP_TRY(auto fileBytes, readBinaryFile(imagePath));
    return std::vector<uint8_t>(
        reinterpret_cast<const uint8_t*>(fileBytes.data()), // NOLINT
        reinterpret_cast<const uint8_t*>(fileBytes.data()) + fileBytes.size()); // NOLINT
}

Result<std::vector<EncodedGltfImage>> collectEncodedImages(
    const tg3_model& model, const std::filesystem::path& baseDir) {
    std::vector<EncodedGltfImage> encodedImages;
    encodedImages.reserve(model.images_count);
    for (auto&& [idx, image] : std::views::enumerate(toSpan(model.images, model.images_count))) {
        auto bytes = resolveImageBytes(model, image, baseDir);
        if (!bytes) {
            return resultError("Failed to resolve GLTF image {}: {}", idx, std::move(bytes).getError());
        }

        CRISP_LOGT("Read image {:>4} '{}', byte size {}.", idx, toStringView(image.name), bytes->size());
        encodedImages.push_back({
            .bytes = std::move(bytes).extract(),
            .name = std::string{toStringView(image.name)},
        });
    }
    return encodedImages;
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
    imageIndicesByHash.reserve(imageLoader.encodedImages.size());
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
    }

    CRISP_CHECK_LE(uniqueImageIndices.size(), std::numeric_limits<uint32_t>::max());
    const uint32_t uniqueImageCount = static_cast<uint32_t>(uniqueImageIndices.size());
    const uint32_t hardwareThreadCount = std::max(1u, std::thread::hardware_concurrency());
    const uint32_t baselineThreadCount = std::min(kBaselineImageDecodeThreadCount, uniqueImageCount);
    const uint32_t workScaledThreadCount = 1 + (uniqueImageCount - 1) / kTargetImagesPerDecodeThread;
    const uint32_t decodeThreadCount =
        std::min({hardwareThreadCount, uniqueImageCount, std::max(baselineThreadCount, workScaledThreadCount)});

    std::vector<std::optional<Image>> decodedImages(uniqueImageIndices.size());
    std::vector<std::string> decodeErrors(uniqueImageIndices.size());
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

    return images;
}

template <GlmAttrib GlmType>
GlmType toGlm(const std::span<const double> values) {
    CRISP_CHECK_EQ(values.size(), GlmType::length());
    GlmType glmValue{};
    for (glm::length_t k = 0; k < GlmType::length(); ++k) {
        glmValue[k] = static_cast<typename GlmType::value_type>(values[k]);
    }
    return glmValue;
}

glm::vec3 getNodeTranslation(const tg3_node& node) {
    return toGlm<glm::vec3>(node.translation);
}

glm::quat getNodeRotation(const tg3_node& node) {
    return toGlm<glm::quat>(node.rotation);
}

glm::vec3 getNodeScale(const tg3_node& node) {
    return toGlm<glm::vec3>(node.scale);
}

glm::mat4 getNodeTransform(const tg3_node& node) {
    // glTF nodes carry either a matrix or a TRS triple, and the parser defaults the unused one to identity.
    if (node.has_matrix != 0) {
        glm::mat4 transform(1.0f);
        for (int32_t i = 0; i < 16; ++i) {
            transform[i / 4][i % 4] = static_cast<float>(node.matrix[i]); // NOLINT
        }
        return transform;
    }

    return glm::translate(getNodeTranslation(node)) * glm::toMat4(getNodeRotation(node)) *
           glm::scale(getNodeScale(node));
}

} // namespace

PbrMaterial createPbrMaterialFromGltfMaterial(
    const tg3_model& model,
    const tg3_material& material,
    GltfImageLoader& loader,
    MaterialWarningCounts& warningCounts) {
    PbrMaterial pbrMaterial{.name = std::string{toStringView(material.name)}};
    if (equalsCStr(material.alpha_mode, "MASK")) {
        pbrMaterial.params.flags |= PbrMaterialAlphaMask;
        pbrMaterial.params.alphaCutoff = static_cast<float>(material.alpha_cutoff);
    } else if (equalsCStr(material.alpha_mode, "BLEND")) {
        ++warningCounts.blendMaterialCount;
    }
    if (material.double_sided != 0) {
        pbrMaterial.params.flags |= PbrMaterialDoubleSided;
    }

    const std::array textureCoordinateSets{
        material.pbr_metallic_roughness.base_color_texture.tex_coord,
        material.pbr_metallic_roughness.metallic_roughness_texture.tex_coord,
        material.normal_texture.tex_coord,
        material.occlusion_texture.tex_coord,
        material.emissive_texture.tex_coord,
    };
    if (std::ranges::any_of(textureCoordinateSets, [](const int32_t texCoord) { return texCoord != 0; })) {
        ++warningCounts.unsupportedTexCoordMaterialCount;
    }

    const auto textures = toSpan(model.textures, model.textures_count);
    const std::array textureIndices{
        material.pbr_metallic_roughness.base_color_texture.index,
        material.pbr_metallic_roughness.metallic_roughness_texture.index,
        material.normal_texture.index,
        material.occlusion_texture.index,
        material.emissive_texture.index,
    };
    if (std::ranges::any_of(textureIndices, [textures](const int32_t textureIndex) {
            return isValidGltfIndex(textureIndex) && isValidGltfIndex(textures[textureIndex].sampler);
        })) {
        ++warningCounts.customSamplerMaterialCount;
    }

    const auto getImageIndex = [textures, &loader](const int32_t textureIndex) -> std::optional<uint32_t> {
        if (!isValidGltfIndex(textureIndex)) {
            return std::nullopt;
        }
        const int32_t sourceIndex = textures[textureIndex].source;
        CRISP_CHECK(isValidGltfIndex(sourceIndex));
        return loader.sourceToLoadedImage.at(sourceIndex);
    };
    const auto setTexture = [&loader, &pbrMaterial, &getImageIndex](const uint32_t mapIndex, const int32_t textureIndex) {
        if (const auto imageIndex = getImageIndex(textureIndex)) {
            pbrMaterial.textureKeys[mapIndex] = fmt::format("{}", *imageIndex);
            loader.loadedImages[*imageIndex].accessTypes[mapIndex] = true;
        }
    };

    setTexture(kPbrAlbedoMapIndex, material.pbr_metallic_roughness.base_color_texture.index);
    setTexture(kPbrNormalMapIndex, material.normal_texture.index);
    setTexture(kPbrEmissiveMapIndex, material.emissive_texture.index);

    const auto metallicRoughnessImage = getImageIndex(material.pbr_metallic_roughness.metallic_roughness_texture.index);
    const auto occlusionImage = getImageIndex(material.occlusion_texture.index);
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
            loader.ormImages.push_back(createPbrOrmMap({
                .occlusion = occlusion,
                .occlusionChannel = 0,
                .roughness = metallicRoughness,
                .roughnessChannel = 1,
                .metallic = metallicRoughness,
                .metallicChannel = 2,
            }));
            ormImage = loader.ormImageIndices.emplace(ormSourceKey, ormImageIndex).first;
        }
        pbrMaterial.textureKeys[kPbrOrmMapIndex] = fmt::format("{}", ormImage->second);
    }

    const glm::vec4 baseColorFactor = toGlm<glm::vec4>(material.pbr_metallic_roughness.base_color_factor);
    pbrMaterial.params.surface.baseColor = glm::vec3(baseColorFactor);
    pbrMaterial.params.geometryOpacity = baseColorFactor.a;

    const glm::vec3 emissiveFactor = toGlm<glm::vec3>(material.emissive_factor);
    pbrMaterial.params.surface.emissionLuminance = std::max({emissiveFactor.r, emissiveFactor.g, emissiveFactor.b});
    pbrMaterial.params.surface.emissionColor =
        pbrMaterial.params.surface.emissionLuminance > 0.0f
            ? emissiveFactor / pbrMaterial.params.surface.emissionLuminance
            : glm::vec3(1.0f);
    pbrMaterial.params.normalScale = static_cast<float>(material.normal_texture.scale);
    pbrMaterial.params.surface.baseMetalness = static_cast<float>(material.pbr_metallic_roughness.metallic_factor);
    pbrMaterial.params.surface.specularRoughness = static_cast<float>(material.pbr_metallic_roughness.roughness_factor);
    pbrMaterial.params.aoStrength = static_cast<float>(material.occlusion_texture.strength);

    return pbrMaterial;
}

TriangleMesh createMeshFromPrimitive(const tg3_model& model, const tg3_primitive& primitive) {
    const int32_t mode = primitive.mode == TG3_INDEX_NONE ? TG3_MODE_TRIANGLES : primitive.mode;
    CRISP_CHECK_EQ(mode, TG3_MODE_TRIANGLES);

    if (primitive.targets_count != 0) {
        CRISP_LOGI("Encountered morph targets in primitive will be skipped.");
    }

    std::vector<glm::vec3> positions{createBuffer<glm::vec3>(model, primitive, "POSITION").unwrap()};
    std::vector<glm::vec3> normals{createBuffer<glm::vec3>(model, primitive, "NORMAL").unwrap()};
    std::vector<glm::vec2> texCoords{createBuffer<glm::vec2>(model, primitive, "TEXCOORD_0").unwrap()};
    std::vector<glm::vec4> tangents{createBuffer<glm::vec4>(model, primitive, "TANGENT").unwrap()};

    std::vector<glm::uvec3> indices;
    if (isValidGltfIndex(primitive.indices)) {
        const auto accessors = toSpan(model.accessors, model.accessors_count);
        CRISP_CHECK_INDEX(primitive.indices, accessors);
        indices = loadIndexBuffer(model, accessors[primitive.indices]).unwrap();
    } else {
        CRISP_CHECK_EQ(positions.size() % 3, 0, "Non-indexed triangle primitives require a multiple of 3 vertices.");
        const auto vertexCount = checkedCast<uint32_t>(positions.size());
        indices.reserve(vertexCount / 3);
        for (uint32_t vertex = 0; vertex < vertexCount; vertex += 3) {
            indices.emplace_back(vertex, vertex + 1, vertex + 2);
        }
    }

    return TriangleMesh{
        std::move(positions), std::move(normals), std::move(texCoords), std::move(indices), std::move(tangents)};
}

Result<std::vector<glm::mat4>> loadInverseBindTransforms(const tg3_model& model, const uint32_t accessorIdx) {
    return createBuffer<glm::mat4>(model, model.accessors[accessorIdx]);
}

SkinningData createSkinningData(const tg3_model& model, const tg3_skin& skin) {
    SkinningData skinningData{};
    const auto joints = toSpan(skin.joints, skin.joints_count);
    const size_t jointCount{joints.size()};
    FlatHashMap<int32_t, int32_t> modelNodeToLocalIdx(jointCount);
    Skeleton skeleton{};
    skeleton.setJointCount(jointCount);
    for (uint32_t i = 0; i < jointCount; ++i) {
        const int32_t jointNodeIdx{joints[i]};
        modelNodeToLocalIdx[jointNodeIdx] = i; // NOLINT
        skeleton.joints[i].rotation = getNodeRotation(model.nodes[jointNodeIdx]);
        skeleton.joints[i].translation = getNodeTranslation(model.nodes[jointNodeIdx]);
        skeleton.joints[i].scale = getNodeScale(model.nodes[jointNodeIdx]);
    }
    for (const auto& [modelNodeIdx, localIdx] : modelNodeToLocalIdx) {
        const auto& jointNode = model.nodes[modelNodeIdx];
        for (const auto& child : toSpan(jointNode.children, jointNode.children_count)) {
            const auto childJoint = modelNodeToLocalIdx.find(child);
            if (childJoint != modelNodeToLocalIdx.end()) {
                skeleton.parents[childJoint->second] = localIdx;
            }
        }
    }

    if (isValidGltfIndex(skin.inverse_bind_matrices)) {
        skinningData.inverseBindTransforms = loadInverseBindTransforms(model, skin.inverse_bind_matrices).unwrap();
        CRISP_CHECK_EQ(skinningData.inverseBindTransforms.size(), jointCount);
    } else {
        skinningData.inverseBindTransforms.resize(jointCount, glm::mat4(1.0f));
    }
    skinningData.skeleton = std::move(skeleton);
    skinningData.modelNodeToLinearIdx = std::move(modelNodeToLocalIdx);
    return skinningData;
}

void createModelDataFromNode(
    const tg3_model& model,
    const tg3_node& node,
    const glm::mat4& parentTransform,
    GltfImageLoader& imageLoader,
    MaterialWarningCounts& warningCounts,
    std::vector<std::optional<PbrMaterial>>& materialCache,
    std::vector<ModelData>& models) {
    if (isValidGltfIndex(node.camera)) {
        CRISP_LOGT("Gltf contains camera information which will be unused.");
    }

    std::optional<SkinningData> skinningData;
    if (isValidGltfIndex(node.skin)) {
        skinningData = createSkinningData(model, model.skins[node.skin]);
    }

    const glm::mat4 worldTransform = parentTransform * getNodeTransform(node);

    if (isValidGltfIndex(node.mesh)) {
        const auto& mesh{model.meshes[node.mesh]};
        CRISP_CHECK(node.weights_count == 0, "Morph targets are not supported!");

        for (const auto& primitive : toSpan(mesh.primitives, mesh.primitives_count)) {
            ModelData modelData{};
            modelData.transform = worldTransform;
            if (skinningData) {
                modelData.skinningData = *skinningData;
            }
            modelData.mesh = createMeshFromPrimitive(model, primitive);

            const bool hasWeights = isValidGltfIndex(findAttributeAccessor(primitive, "WEIGHTS_0"));
            const bool hasJoints = isValidGltfIndex(findAttributeAccessor(primitive, "JOINTS_0"));
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
                auto& cachedMaterial = materialCache.at(primitive.material);
                if (!cachedMaterial) {
                    cachedMaterial.emplace(createPbrMaterialFromGltfMaterial(
                        model, model.materials[primitive.material], imageLoader, warningCounts));
                }

                modelData.material = *cachedMaterial;
            } else {
                // glTF's implicit material uses metallic-roughness defaults, not the OpenPBR constructor defaults.
                modelData.material.params.surface.baseColor = glm::vec3(1.0f);
                modelData.material.params.surface.baseMetalness = 1.0f;
                modelData.material.params.surface.specularRoughness = 1.0f;
            }

            models.push_back(std::move(modelData));
        }
    }

    for (const int32_t childIdx : toSpan(node.children, node.children_count)) {
        createModelDataFromNode(
            model, model.nodes[childIdx], worldTransform, imageLoader, warningCounts, materialCache, models);
    }
}

size_t countModelPrimitives(const tg3_model& model, const int32_t nodeIndex) {
    const auto& node = model.nodes[nodeIndex];
    size_t primitiveCount = isValidGltfIndex(node.mesh) ? model.meshes[node.mesh].primitives_count : 0;
    for (const int32_t childIndex : toSpan(node.children, node.children_count)) {
        primitiveCount += countModelPrimitives(model, childIndex);
    }
    return primitiveCount;
}

AnimationData createAnimationData(const tg3_model& model, const tg3_animation& animation) {
    AnimationData anim;
    const auto channels = toSpan(animation.channels, animation.channels_count);
    const auto samplers = toSpan(animation.samplers, animation.samplers_count);
    anim.channels.reserve(channels.size());
    for (const auto& ch : channels) {
        AnimationChannel channel{};
        channel.targetNode = ch.target.node;
        channel.propertyName = std::string{toStringView(ch.target.path)};

        const auto& sampler{samplers[ch.sampler]};
        if (equalsCStr(sampler.interpolation, "CUBICSPLINE")) {
            channel.sampler.interpolation = AnimationSampler::Interpolation::CubicSpline;
        } else if (equalsCStr(sampler.interpolation, "STEP")) {
            channel.sampler.interpolation = AnimationSampler::Interpolation::Step;
        } else {
            channel.sampler.interpolation = AnimationSampler::Interpolation::Linear;
        }
        channel.sampler.inputs = createBuffer<float>(model, model.accessors[sampler.input]).unwrap();
        if (channel.propertyName == "translation") {
            auto vals = createBuffer<glm::vec3>(model, model.accessors[sampler.output]).unwrap();
            channel.sampler.outputs.resize(vals.size() * sizeof(glm::vec3));
            memcpy(channel.sampler.outputs.data(), vals.data(), vals.size() * sizeof(glm::vec3));
        } else if (channel.propertyName == "rotation") {
            auto vals = createBuffer<glm::quat>(model, model.accessors[sampler.output]).unwrap();
            channel.sampler.outputs.resize(vals.size() * sizeof(glm::quat));
            memcpy(channel.sampler.outputs.data(), vals.data(), vals.size() * sizeof(glm::quat));
        } else {
            auto vals = createBuffer<glm::vec3>(model, model.accessors[sampler.output]).unwrap();
            channel.sampler.outputs.resize(vals.size() * sizeof(glm::vec3));
            memcpy(channel.sampler.outputs.data(), vals.data(), vals.size() * sizeof(glm::vec3));
        }
        anim.channels.push_back(std::move(channel));
    }

    return anim;
}

PbrImageGroup createPbrImageData(
    std::string name, const std::span<ImageData> images, std::vector<Image> ormImages, const std::span<ModelData> models) {
    PbrImageGroup imageData{.name = std::move(name), .ormMaps = std::move(ormImages)};
    imageData.albedoMaps.reserve(std::ranges::count_if(images, [](const ImageData& image) {
        return image.accessTypes[kPbrAlbedoMapIndex];
    }));
    imageData.normalMaps.reserve(std::ranges::count_if(images, [](const ImageData& image) {
        return image.accessTypes[kPbrNormalMapIndex];
    }));
    imageData.emissiveMaps.reserve(std::ranges::count_if(images, [](const ImageData& image) {
        return image.accessTypes[kPbrEmissiveMapIndex];
    }));
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

    tg3_parse_options options{};
    tg3_parse_options_init(&options);

    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;

    const auto loadStart = std::chrono::steady_clock::now();
    const auto pathString = path.string();
    const tg3_error_code parseResult = tg3_parse_file(
        model.get(), errors.get(), pathString.c_str(), static_cast<uint32_t>(pathString.size()), &options);

    std::string parseErrors;
    for (uint32_t i = 0; i < errors.count(); ++i) {
        const auto* entry = errors.entry(i);
        const auto message = entry->message != nullptr ? std::string_view{entry->message} : std::string_view{"unknown"};
        if (entry->severity == TG3_SEVERITY_ERROR) {
            parseErrors += fmt::format("{}{}", parseErrors.empty() ? "" : "; ", message);
        } else if (entry->severity == TG3_SEVERITY_WARNING) {
            CRISP_LOGW("GLTF warning from {}: {}", path.string(), message);
        }
    }

    if (!parseErrors.empty()) {
        return resultError("GLTF error from {}: {}", path.string(), parseErrors);
    }
    if (parseResult != TG3_OK) {
        return resultError("Failed to parse GLTF {} (error code {}).", path.string(), static_cast<int32_t>(parseResult));
    }
    if (model->nodes_count == 0) {
        return resultError("Provided GLTF {} is empty!", path.string());
    }

    CRISP_CHECK_EQ(model->scenes_count, 1, "Multi-scene GLTF is unsupported.");
    CRISP_CHECK_EQ(model->default_scene, 0);
    const auto& scene{model->scenes[model->default_scene]};

    const auto imageDecodeStart = std::chrono::steady_clock::now();
    GltfImageLoader imageLoader{};
    CRISP_TRY(imageLoader.encodedImages, collectEncodedImages(*model.get(), path.parent_path()));
    CRISP_TRY(imageLoader.loadedImages, decodeGltfImages(imageLoader));
    const auto imageDecodeDuration = std::chrono::steady_clock::now() - imageDecodeStart;

    const auto modelWorkStart = std::chrono::steady_clock::now();
    const auto sceneNodes = toSpan(scene.nodes, scene.nodes_count);
    size_t modelPrimitiveCount{0};
    for (const int32_t nodeIndex : sceneNodes) {
        modelPrimitiveCount += countModelPrimitives(*model.get(), nodeIndex);
    }

    MaterialWarningCounts materialWarningCounts{};
    SceneData sceneData{};
    std::vector<std::optional<PbrMaterial>> materialCache(model->materials_count);
    sceneData.models.reserve(modelPrimitiveCount);
    imageLoader.ormImages.reserve(model->materials_count);
    imageLoader.ormImageIndices.reserve(model->materials_count);
    for (const int32_t nodeIndex : sceneNodes) {
        createModelDataFromNode(
            *model.get(),
            model->nodes[nodeIndex],
            glm::mat4(1.0f),
            imageLoader,
            materialWarningCounts,
            materialCache,
            sceneData.models);
    }
    std::vector<AnimationData> animations{};
    animations.reserve(model->animations_count);
    for (const auto& animation : toSpan(model->animations, model->animations_count)) {
        animations.push_back(createAnimationData(*model.get(), animation));
    }

    for (auto& sceneModel : sceneData.models) {
        const auto& jointIndices = sceneModel.skinningData.modelNodeToLinearIdx;
        if (jointIndices.empty()) {
            continue;
        }

        sceneModel.animations.reserve(animations.size());
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
    const auto modelWorkDuration = std::chrono::steady_clock::now() - modelWorkStart;

    const auto imageOrganizationStart = std::chrono::steady_clock::now();
    sceneData.images = createPbrImageData(
        path.stem().string(), imageLoader.loadedImages, std::move(imageLoader.ormImages), sceneData.models);
    const auto imageWorkDuration = imageDecodeDuration + (std::chrono::steady_clock::now() - imageOrganizationStart);

    const std::chrono::duration<double, std::milli> totalDuration{std::chrono::steady_clock::now() - loadStart};
    const std::chrono::duration<double, std::milli> imageDuration{imageWorkDuration};
    const std::chrono::duration<double, std::milli> modelDuration{modelWorkDuration};
    CRISP_LOGI(
        "GLTF load timing for '{}': total {:.1f} ms | images {:.1f} ms | models {:.1f} ms.",
        path.filename().string(),
        totalDuration.count(),
        imageDuration.count(),
        modelDuration.count());
    if (materialWarningCounts.blendMaterialCount != 0 || materialWarningCounts.unsupportedTexCoordMaterialCount != 0 ||
        materialWarningCounts.customSamplerMaterialCount != 0) {
        CRISP_LOGW(
            "GLTF referenced-material limitations: {} alpha-blended, {} using texture coordinates "
            "other than TEXCOORD_0, {} using custom samplers. These features are currently ignored; alpha-mask and "
            "double-sided shadow materials are supported.",
            materialWarningCounts.blendMaterialCount,
            materialWarningCounts.unsupportedTexCoordMaterialCount,
            materialWarningCounts.customSamplerMaterialCount);
    }

    return sceneData;
}

} // namespace crisp
