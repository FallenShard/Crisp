#include <Crisp/Mesh/Io/GltfReader.hpp>

#include <Crisp/Image/Io/ImageLoader.hpp>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

namespace crisp
{
namespace
{
const auto logger = createLoggerMt("GltfReader");

constexpr uint32_t MetallicChannel = 2;
constexpr uint32_t RoughnessChannel = 1;

Result<std::vector<glm::uvec3>> loadIndexBuffer(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
{
    std::vector<glm::uvec3> indices;
    const auto& accessor = model.accessors.at(primitive.indices);
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
    {
        auto& bufferView = model.bufferViews.at(accessor.bufferView);
        auto& buffer = model.buffers.at(bufferView.buffer);
        indices.resize(bufferView.byteLength / sizeof(glm::uvec3));

        std::memcpy(indices.data(), buffer.data.data() + bufferView.byteOffset, bufferView.byteLength);
    }
    else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
    {
        auto& bufferView = model.bufferViews.at(accessor.bufferView);
        auto& buffer = model.buffers.at(bufferView.buffer);

        const size_t elementByteSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
        const size_t componentCount = 3;
        // tinygltf::GetNumComponentsInType(accessor.type);
        const size_t triangleCount = bufferView.byteLength / elementByteSize / componentCount;
        indices.reserve(triangleCount);

        for (size_t i = 0; i < triangleCount; ++i)
        {
            glm::u16vec3 temp;
            const size_t offset = bufferView.byteOffset + i * elementByteSize * componentCount;
            std::memcpy(&temp, buffer.data.data() + offset, sizeof(temp));
            indices.emplace_back(temp.x, temp.y, temp.z);
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
    std::vector<T> attributes;
    const auto& accessor = model.accessors.at(primitive.attributes.at(attrib));
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
    {
        const auto& bufferView = model.bufferViews.at(accessor.bufferView);
        const auto& buffer = model.buffers.at(bufferView.buffer);

        const size_t componentByteSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
        const size_t componentCount = tinygltf::GetNumComponentsInType(accessor.type);
        const size_t elementByteSize = componentCount * componentByteSize;

        attributes.reserve(accessor.count);

        for (size_t i = 0; i < accessor.count; ++i)
        {
            T temp;
            const size_t offset = bufferView.byteOffset + i * elementByteSize;
            std::memcpy(&temp, buffer.data.data() + offset, elementByteSize);
            attributes.emplace_back(temp);
        }
    }
    else
    {
        return resultError("Failed to parse vertex buffer for {}", attrib);
    }

    return attributes;
}

struct ImageLoaderContext
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

    ImageLoaderContext& context = *static_cast<ImageLoaderContext*>(userPtr);
    const std::filesystem::path fullPath{context.baseDir / image->uri};
    // context.loadedImages.emplace_back(image->uri, loadImage(std::span(bytes, size)).unwrap());
    context.loadedImages.emplace_back(image->uri, loadImage(fullPath).unwrap());
    return true;
}

} // namespace

Result<std::pair<TriangleMesh, PbrTextureGroup>> loadGltfModel(
    const std::filesystem::path& path, const std::vector<VertexAttributeDescriptor>& vertexAttributes)
{
    tinygltf::Model model;

    tinygltf::TinyGLTF loader;
    ImageLoaderContext loaderCtx{path.parent_path()};
    loader.SetImageLoader(loadImageFromGltf, &loaderCtx);

    std::string err;
    std::string warn;
    const bool success = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());

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

    // We assume the first node is the one that we are loading for now.
    const auto& mainNode = model.nodes.at(0);

    // We also assume the mesh has only 1 primitive.
    const auto& prim = model.meshes.at(mainNode.mesh).primitives.at(0);

    std::vector<glm::uvec3> indices = loadIndexBuffer(model, prim).unwrap();
    std::vector<glm::vec3> positions = loadVertexBuffer<glm::vec3>(model, prim, "POSITION").unwrap();
    std::vector<glm::vec3> normals = loadVertexBuffer<glm::vec3>(model, prim, "NORMAL").unwrap();
    std::vector<glm::vec2> texCoords = loadVertexBuffer<glm::vec2>(model, prim, "TEXCOORD_0").unwrap();

    const auto& material = model.materials.at(prim.material);

    PbrTextureGroup texGroup{};
    // texGroup.materialKey = model.nodes.at(0).name;
    texGroup.albedo = std::move(loaderCtx.loadedImages.at(material.pbrMetallicRoughness.baseColorTexture.index).second);
    texGroup.normal = std::move(loaderCtx.loadedImages.at(material.normalTexture.index).second);
    texGroup.metallic = loaderCtx.loadedImages.at(material.pbrMetallicRoughness.metallicRoughnessTexture.index)
                            .second.createFromChannel(MetallicChannel);
    texGroup.roughness = loaderCtx.loadedImages.at(material.pbrMetallicRoughness.metallicRoughnessTexture.index)
                             .second.createFromChannel(RoughnessChannel);
    texGroup.occlusion = std::move(loaderCtx.loadedImages.at(material.occlusionTexture.index).second);
    texGroup.emissive = std::move(loaderCtx.loadedImages.at(material.emissiveTexture.index).second);

    TriangleMesh mesh{
        std::move(positions), std::move(normals), std::move(texCoords), std::move(indices), vertexAttributes};

    glm::mat4 transform(1.0f);
    if (mainNode.rotation.size() == glm::quat::length())
    {
        const auto& quatValues{mainNode.rotation};
        glm::quat quat{};
        for (glm::length_t i = 0; i < glm::quat::length(); ++i)
        {
            quat[i] = static_cast<float>(quatValues[i]);
        }
        transform = glm::toMat4(quat);
    }
    mesh.transform(transform);

    return std::make_pair(std::move(mesh), std::move(texGroup));
}

} // namespace crisp
