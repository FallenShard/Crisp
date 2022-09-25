#include <Crisp/IO/WavefrontObjReader.hpp>

#include <Crisp/Common/Logger.hpp>
#include <Crisp/Utils/StringUtils.hpp>

#include <array>
#include <charconv>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace crisp
{
namespace
{
struct ObjVertex
{
    using IndexType = int32_t;
    IndexType p = -1;
    std::optional<IndexType> n;
    std::optional<IndexType> uv;

    inline ObjVertex(){};

    inline ObjVertex(std::string_view stringView)
    {
        auto vertexAttribs = fixedTokenize<3>(stringView, "/", false);

        auto parse = [](const std::string_view& line_view)
        {
            IndexType val;
            std::from_chars(line_view.data(), line_view.data() + line_view.size(), val);
            return val < 0 ? val : val - 1; // return negative if it is a negative value, otherwise 0-based
        };

        if (!vertexAttribs[0].empty())
            p = parse(vertexAttribs[0]);
        else
            spdlog::critical("Invalid vertex specification in an obj file.");

        if (!vertexAttribs[1].empty())
            uv = parse(vertexAttribs[1]);

        if (!vertexAttribs[2].empty())
            n = parse(vertexAttribs[2]);
    }

    inline bool operator==(const ObjVertex& v) const
    {
        return p == v.p && uv == v.uv && n == v.n;
    }
};

struct ObjVertexHasher
{
    std::size_t operator()(const ObjVertex& v) const
    {
        size_t hash = std::hash<ObjVertex::IndexType>()(v.p);

        if (v.uv)
            hash = hash * 37 + std::hash<ObjVertex::IndexType>()(v.uv.value());

        if (v.n)
            hash = hash * 37 + std::hash<ObjVertex::IndexType>()(v.n.value());

        return hash;
    }
};

FlatHashMap<std::string, WavefrontObjMaterial> loadMaterials(const std::filesystem::path& path)
{
    FlatHashMap<std::string, WavefrontObjMaterial> materials;

    WavefrontObjMaterial* currMat = nullptr;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line))
    {
        const std::size_t prefixEnd = line.find_first_of(' ', 0);
        const std::string_view prefix = std::string_view(line).substr(0, prefixEnd);

        if (prefix == "newmtl")
        {
            auto tokens = fixedTokenize<2>(line, " ");
            currMat = &materials[std::string(tokens[1])];
        }
        else if (prefix == "Ns")
        {
            auto tokens = fixedTokenize<2>(line, " ");
            if (currMat)
                std::from_chars(tokens[1].data(), tokens[1].data() + tokens[1].size(), currMat->ns);
        }
        else if (prefix == "Ka")
        {
            auto tokens = fixedTokenize<4>(line, " ");
            if (currMat)
                for (int i = 0; i < decltype(currMat->ambient)::length(); ++i)
                    std::from_chars(
                        tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), currMat->ambient[i]);
        }
        else if (prefix == "Kd")
        {
            auto tokens = fixedTokenize<4>(line, " ");
            if (currMat)
                for (int i = 0; i < decltype(currMat->diffuse)::length(); ++i)
                    std::from_chars(
                        tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), currMat->diffuse[i]);
        }
        else if (prefix == "Ks")
        {
            auto tokens = fixedTokenize<4>(line, " ");
            if (currMat)
                for (int i = 0; i < decltype(currMat->specular)::length(); ++i)
                    std::from_chars(
                        tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), currMat->specular[i]);
        }
        else if (prefix == "Ni")
        {
            auto tokens = fixedTokenize<2>(line, " ");
            if (currMat)
                std::from_chars(tokens[1].data(), tokens[1].data() + tokens[1].size(), currMat->ni);
        }
        else if (prefix == "d")
        {
            auto tokens = fixedTokenize<2>(line, " ");
            if (currMat)
                std::from_chars(tokens[1].data(), tokens[1].data() + tokens[1].size(), currMat->d);
        }
        else if (prefix == "illum")
        {
            auto tokens = fixedTokenize<2>(line, " ");
            if (currMat)
                std::from_chars(tokens[1].data(), tokens[1].data() + tokens[1].size(), currMat->illumination);
        }
        else if (prefix == "map_Kd")
        {
            auto tokens = fixedTokenize<2>(line, " ");
            if (currMat)
                currMat->albedoMap = std::string(tokens[1]);
        }
        else if (prefix == "map_Bump")
        {
            auto tokens = fixedTokenize<2>(line, " ");
            if (currMat)
                currMat->normalMap = std::string(tokens[1]);
        }
        else if (prefix == "map_Ks")
        {
            auto tokens = fixedTokenize<2>(line, " ");
            if (currMat)
                currMat->specularMap = std::string(tokens[1]);
        }
    }

    return materials;
}

template <typename GlmVecType>
GlmVecType parseAttribute(const std::string& line)
{
    GlmVecType attrib{};
    const auto tokens = fixedTokenize<GlmVecType::length() + 1>(line, " ");
    for (glm::length_t i = 0; i < GlmVecType::length(); ++i)
        std::from_chars(tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), attrib[i]);
    return attrib;
};
} // namespace

bool isWavefrontObjFile(const std::filesystem::path& path)
{
    return path.extension().string() == ".obj";
}

WavefrontObjMesh readWavefrontObj(const std::filesystem::path& objFilePath)
{
    std::ifstream file(objFilePath);

    robin_hood::unordered_flat_map<ObjVertex, uint32_t, ObjVertexHasher> vertexMap;
    std::vector<glm::vec3> positionList;
    std::vector<glm::vec3> normalList;
    std::vector<glm::vec2> texCoordList;
    std::vector<glm::vec3> paramList;
    std::vector<glm::uvec3> triangles;
    std::vector<TriangleMeshView> meshViews;
    robin_hood::unordered_flat_map<std::string, WavefrontObjMaterial> materials;

    std::string line;
    uint32_t uniqueVertexId = 0;
    while (std::getline(file, line))
    {
        const std::size_t prefixEnd = line.find(' ', 0);
        const std::string_view prefix = std::string_view(line).substr(0, prefixEnd);

        if (prefix == "o")
        {
            const auto tokens = fixedTokenize<2>(line, " ");
            meshViews.push_back(
                TriangleMeshView{std::string(tokens[1]), static_cast<uint32_t>(3 * triangles.size()), 0});
        }
        else if (prefix == "mtllib")
        {
            const auto tokens = fixedTokenize<2>(line, " ");
            const std::filesystem::path materialFilePath = objFilePath.parent_path() / tokens[1];
            if (std::filesystem::exists(materialFilePath))
                materials = loadMaterials(materialFilePath);
        }
        else if (prefix == "v")
        {
            positionList.push_back(parseAttribute<glm::vec3>(line));
        }
        else if (prefix == "vt")
        {
            texCoordList.push_back(parseAttribute<glm::vec2>(line));
        }
        else if (prefix == "vn")
        {
            normalList.push_back(glm::normalize(parseAttribute<glm::vec3>(line)));
        }
        else if (prefix == "vp")
        {
            paramList.push_back(parseAttribute<glm::vec3>(line));
        }
        else if (prefix == "f")
        {
            glm::uvec3 faceIndices;
            const auto tokens = fixedTokenize<4>(line, " ");

            for (int i = 0; i < 3; i++)
            {
                const ObjVertex vertex(tokens[i + 1]);
                const auto it = vertexMap.find(vertex);
                if (it == vertexMap.end())
                {
                    vertexMap[vertex] = uniqueVertexId;
                    faceIndices[i] = uniqueVertexId;
                    ++uniqueVertexId;
                }
                else
                {
                    faceIndices[i] = it->second;
                }
            }

            triangles.push_back(faceIndices);
        }
        else if (prefix == "usemtl")
        {
            const auto tokens = fixedTokenize<2>(line, " ");
            meshViews.push_back(
                TriangleMeshView{std::string(tokens[1]), static_cast<uint32_t>(3 * triangles.size()), 0});
        }
        else if (prefix == "#")
        {
            // comment;
        }
    }

    triangles.shrink_to_fit();

    WavefrontObjMesh mesh;
    mesh.triangles = std::move(triangles);
    mesh.positions.resize(positionList.empty() ? 0 : vertexMap.size());
    mesh.normals.resize(normalList.empty() ? 0 : vertexMap.size());
    mesh.texCoords.resize(texCoordList.empty() ? 0 : vertexMap.size());

    for (auto& [attribIndices, vertexIdx] : vertexMap)
    {
        if (attribIndices.p < 0)
            attribIndices.p = static_cast<ObjVertex::IndexType>(positionList.size()) + attribIndices.p;

        mesh.positions[vertexIdx] = positionList[attribIndices.p];

        if (attribIndices.n)
        {
            ObjVertex::IndexType n = attribIndices.n.value();
            if (n < 0)
                n = static_cast<ObjVertex::IndexType>(normalList.size()) + n;
            mesh.normals[vertexIdx] = normalList[n];
        }

        if (attribIndices.uv)
        {
            ObjVertex::IndexType uv = attribIndices.uv.value();
            if (uv < 0)
                uv = static_cast<ObjVertex::IndexType>(texCoordList.size()) + uv;
            mesh.texCoords[vertexIdx] = texCoordList[uv];
        }
    }

    if (meshViews.size() >= 1)
    {
        for (std::size_t i = 0; i < meshViews.size() - 1; ++i)
            meshViews[i].count = meshViews[i + 1].first - meshViews[i].first;

        meshViews.back().count = static_cast<uint32_t>(3 * mesh.triangles.size() - meshViews.back().first);
    }

    meshViews.erase(
        std::remove_if(
            meshViews.begin(),
            meshViews.end(),
            [](const TriangleMeshView& part)
            {
                return part.count == 0;
            }),
        meshViews.end());

    mesh.views = std::move(meshViews);
    mesh.materials = std::move(materials);
    return mesh;
}

} // namespace crisp
