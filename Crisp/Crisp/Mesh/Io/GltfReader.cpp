#include <Crisp/Mesh/Io/GltfReader.hpp>

#include <Crisp/Common/Checks.hpp>
#include <Crisp/Image/Io/Utils.hpp>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

namespace crisp
{
namespace
{
const auto logger = createLoggerMt("GltfReader");

constexpr uint32_t OcclusionMapChannel{0};
constexpr uint32_t RoughnessMapChannel{1};
constexpr uint32_t MetallicMapChannel{2};

constexpr int32_t GltfInvalidIdx{-1};

Result<std::vector<glm::uvec3>> loadIndexBuffer(const tinygltf::Model& model, uint32_t indicesAccessorIdx)
{
    const auto& accessor{model.accessors.at(indicesAccessorIdx)};
    const auto& bufferView{model.bufferViews.at(accessor.bufferView)};
    const auto& buffer{model.buffers.at(bufferView.buffer)};

    constexpr size_t ComponentCount{3}; // Triangular mesh only.
    const int32_t elementByteSize{tinygltf::GetComponentSizeInBytes(accessor.componentType)};
    const size_t triangleByteCount{elementByteSize * ComponentCount};
    const size_t triangleCount{bufferView.byteLength / triangleByteCount};

    std::vector<glm::uvec3> indices(triangleCount);
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
    {
        std::memcpy(indices.data(), buffer.data.data() + bufferView.byteOffset, bufferView.byteLength);
    }
    else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
    {
        glm::u16vec3 temp{};
        for (size_t i = 0; i < triangleCount; ++i)
        {
            const size_t offset{bufferView.byteOffset + i * triangleByteCount};
            std::memcpy(&temp, buffer.data.data() + offset, triangleByteCount);
            indices[i] = glm::uvec3(temp.x, temp.y, temp.z);
        }
    }
    else
    {
        return resultError("Failed to parse index buffer!");
    }

    return indices;
}

template <typename T>
Result<std::vector<T>> loadVertexBuffer(
    const tinygltf::Model& model, const tinygltf::Primitive& primitive, const std::string& attrib)
{
    if (!primitive.attributes.contains(attrib))
    {
        return std::vector<T>{};
    }

    std::vector<T> attributes;
    const auto& accessor = model.accessors.at(primitive.attributes.at(attrib));
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
    {
        const auto& bufferView = model.bufferViews.at(accessor.bufferView);
        const auto& buffer = model.buffers.at(bufferView.buffer);

        const size_t componentByteSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
        const size_t componentCount = tinygltf::GetNumComponentsInType(accessor.type);
        const size_t attributeByteSize{componentCount * componentByteSize};
        const size_t byteStride{bufferView.byteStride == 0 ? attributeByteSize : bufferView.byteStride};

        attributes.reserve(accessor.count);

        const size_t bufferRangeStart{bufferView.byteOffset + accessor.byteOffset};
        const size_t bufferRangeEnd{bufferRangeStart + accessor.count * byteStride};
        CRISP_CHECK(bufferRangeStart <= buffer.data.size());
        CRISP_CHECK(bufferRangeEnd <= buffer.data.size());

        T temp{};
        for (size_t i = 0; i < accessor.count; ++i)
        {
            const size_t offset{bufferRangeStart + i * byteStride};
            std::memcpy(&temp, buffer.data.data() + offset, attributeByteSize);
            attributes.emplace_back(temp);
        }
    }
    else
    {
        return resultError("Failed to parse vertex buffer for {}", attrib);
    }

    return attributes;
}

struct GltfImageLoader
{
    const std::filesystem::path baseDir;
    std::vector<std::pair<std::string, Image>> loadedImages;
};

bool loadImageFromGltf(
    tinygltf::Image* image,
    const int32_t imageIdx,
    std::string* err,
    std::string* warn,
    const int32_t reqWidth,
    const int32_t reqHeight,
    const uint8_t* bytes,
    const int32_t size,
    void* userPtr)
{
    logger->info("Index {}, Uri: {}, required size: {} x {}", imageIdx, image->uri, reqWidth, reqHeight);
    logger->info("Size {}, bytes {}", size, bytes ? "available" : "empty");
    if (err && !err->empty())
    {
        logger->error("Error while loading GLTF texture: {}", *err);
    }
    if (warn && !warn->empty())
    {
        logger->warn("Warning while loading GLTF texture: {}", *warn);
    }

    GltfImageLoader& imageLoader{*static_cast<GltfImageLoader*>(userPtr)};
    const std::filesystem::path fullPath{imageLoader.baseDir / image->uri};
    imageLoader.loadedImages.emplace_back(image->uri, loadImage(fullPath).unwrap());
    return true;
}

} // namespace

template <typename GlmType>
GlmType toGlm(const std::vector<double>& values)
{
    CRISP_CHECK_EQ(values.size(), GlmType::length());
    GlmType glmValue{};
    for (glm::length_t k = 0; k < GlmType::length(); ++k)
    {
        glmValue[k] = static_cast<float>(values[k]);
    }
    return glmValue;
}

glm::mat4 getNodeTransform(const tinygltf::Node& node)
{
    glm::mat4 transform(1.0f);
    if (!node.matrix.empty())
    {
        CRISP_CHECK_EQ(node.matrix.size(), 16);
        for (uint32_t i = 0; i < node.matrix.size(); ++i)
        {
            transform[i / 4][i % 4] = static_cast<float>(node.matrix[i]);
        }
    }
    if (!node.translation.empty())
    {
        transform = transform * glm::translate(toGlm<glm::vec3>(node.translation));
    }
    if (!node.rotation.empty())
    {
        transform = transform * glm::toMat4(toGlm<glm::quat>(node.rotation));
    }
    if (!node.scale.empty())
    {
        transform = transform * glm::scale(toGlm<glm::vec3>(node.scale));
    }
    return transform;
}

PbrMaterial createPbrMaterialFromGltfMaterial(const tinygltf::Material& material, GltfImageLoader& imageLoader)
{
    PbrMaterial pbrMat{};

    const auto getTexture = [&imageLoader](std::optional<Image>& image, const int32_t textureIndex)
    {
        if (textureIndex != GltfInvalidIdx)
        {
            image = imageLoader.loadedImages.at(textureIndex).second;
        }
    };

    // Base color (albedo).
    getTexture(pbrMat.textures.albedo, material.pbrMetallicRoughness.baseColorTexture.index);
    pbrMat.params.albedo = toGlm<glm::vec4>(material.pbrMetallicRoughness.baseColorFactor);

    getTexture(pbrMat.textures.normal, material.normalTexture.index);
    getTexture(pbrMat.textures.emissive, material.emissiveTexture.index);

    // Metallic and roughness.
    if (material.pbrMetallicRoughness.metallicRoughnessTexture.index != GltfInvalidIdx)
    {
        const auto& imageNamePair{
            imageLoader.loadedImages.at(material.pbrMetallicRoughness.metallicRoughnessTexture.index)};

        pbrMat.textures.roughness = imageNamePair.second.createFromChannel(RoughnessMapChannel);
        pbrMat.textures.metallic = imageNamePair.second.createFromChannel(MetallicMapChannel);
    }
    pbrMat.params.metallic = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
    pbrMat.params.roughness = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);

    // Occlusion.
    if (material.occlusionTexture.index != GltfInvalidIdx)
    {
        pbrMat.textures.occlusion =
            imageLoader.loadedImages.at(material.occlusionTexture.index).second.createFromChannel(OcclusionMapChannel);
    }
    pbrMat.params.aoStrength = static_cast<float>(material.occlusionTexture.strength);

    return pbrMat;
}

TriangleMesh createMeshFromPrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const std::vector<VertexAttributeDescriptor>& vertexAttributes)
{
    std::vector<glm::vec3> positions{loadVertexBuffer<glm::vec3>(model, primitive, "POSITION").unwrap()};
    std::vector<glm::vec3> normals{loadVertexBuffer<glm::vec3>(model, primitive, "NORMAL").unwrap()};
    std::vector<glm::vec2> texCoords{loadVertexBuffer<glm::vec2>(model, primitive, "TEXCOORD_0").unwrap()};
    std::vector<glm::vec4> tangents{loadVertexBuffer<glm::vec4>(model, primitive, "TANGENT").unwrap()};

    std::vector<glm::uvec3> indices{loadIndexBuffer(model, primitive.indices).unwrap()};

    return TriangleMesh{
        std::move(positions), std::move(normals), std::move(texCoords), std::move(indices), vertexAttributes};
}

void createRenderObjectsFromNode(
    const tinygltf::Model& model,
    const tinygltf::Node& node,
    GltfImageLoader& imageLoader,
    const std::vector<VertexAttributeDescriptor>& vertexAttributes,
    std::vector<RenderObject>& renderObjects)
{
    CRISP_CHECK_EQ(node.skin, GltfInvalidIdx, "Skinning is unsupported!");
    // CRISP_CHECK_EQ(node.camera, GltfInvalidIdx, "Camera is unsupported!");

    if (node.mesh != GltfInvalidIdx)
    {
        const auto& currMesh{model.meshes.at(node.mesh)};
        CRISP_CHECK(node.weights.empty(), "Morph targets are not supported!");

        // We also assume the mesh has only 1 primitive.
        const auto& currPrimitive{currMesh.primitives.at(0)};

        RenderObject renderObject{};
        renderObject.mesh = createMeshFromPrimitive(model, currPrimitive, vertexAttributes);
        renderObject.material =
            createPbrMaterialFromGltfMaterial(model.materials.at(currPrimitive.material), imageLoader);
        renderObject.transform = getNodeTransform(node);

        renderObjects.push_back(std::move(renderObject));
    }

    for (const uint32_t childIdx : node.children)
    {
        createRenderObjectsFromNode(model, model.nodes.at(childIdx), imageLoader, vertexAttributes, renderObjects);
    }
}

Result<std::vector<RenderObject>> loadGltfModel(
    const std::filesystem::path& path, const std::vector<VertexAttributeDescriptor>& vertexAttributes)
{
    tinygltf::Model model;

    tinygltf::TinyGLTF loader;
    GltfImageLoader imageLoader{path.parent_path()};
    loader.SetImageLoader(loadImageFromGltf, &imageLoader);

    std::string err{};
    std::string warn{};
    const bool success{loader.LoadASCIIFromFile(&model, &err, &warn, path.string())};

    if (!warn.empty())
    {
        logger->warn("GLTF warning from {}: {}", path.string(), warn);
    }
    if (!err.empty())
    {
        return resultError("GLTF error from {}: {}", path.string(), err);
    }
    if (!success)
    {
        return resultError("Failed to parse GLTF: {}!", path.string());
    }

    if (model.nodes.empty())
    {
        return resultError("Provided GLTF {} is empty!", path.string());
    }

    CRISP_CHECK_EQ(model.scenes.size(), 1, "Multi-scene gltf is unsupported.");
    CRISP_CHECK_EQ(model.defaultScene, 0);
    const auto& gltfScene{model.scenes.at(model.defaultScene)};

    std::vector<RenderObject> renderObjects{};
    for (uint32_t i = 0; i < gltfScene.nodes.size(); ++i)
    {
        createRenderObjectsFromNode(model, model.nodes[i], imageLoader, vertexAttributes, renderObjects);
    }

    return renderObjects;
}

} // namespace crisp
