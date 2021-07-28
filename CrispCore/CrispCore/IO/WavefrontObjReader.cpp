#include "WavefrontObjReader.hpp"

#include "CrispCore/StringUtils.hpp"

#include <spdlog/spdlog.h>

#pragma warning(push)
#pragma warning(disable: 26819) // Fallthrough
#include <robin_hood/robin_hood.h>
#pragma warning(pop)

#include <iostream>
#include <sstream>
#include <charconv>
#include <array>
#include <string_view>
#include <fstream>
#include <string>
#include <unordered_set>
#include <optional>

namespace crisp
{
    namespace
    {
        struct ObjVertex
        {
            int64_t p = -1;
            std::optional<int64_t> n;
            std::optional<int64_t> uv;

            inline ObjVertex() {};
            inline ObjVertex(std::string_view stringView)
            {
                auto vertexAttribs = fixedTokenize<3>(stringView, "/", false);

                auto parse = [](const std::string_view& line_view)
                {
                    int64_t val;
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
                using IndexType = int64_t;
                size_t hash = std::hash<IndexType>()(v.p);

                if (v.uv)
                    hash = hash * 37 + std::hash<IndexType>()(v.uv.value());

                if (v.n)
                    hash = hash * 37 + std::hash<IndexType>()(v.n.value());

                return hash;
            }
        };

        std::unordered_map<std::string, WavefrontObjReader::Material> loadMaterials(const std::filesystem::path& path)
        {
            std::unordered_map<std::string, WavefrontObjReader::Material> materials;

            WavefrontObjReader::Material* currMat = nullptr;
            std::ifstream file(path);
            std::string line;
            while (std::getline(file, line))
            {
                std::size_t prefixEnd = line.find_first_of(' ', 0);
                std::string_view prefix = std::string_view(line).substr(0, prefixEnd);

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
                            std::from_chars(tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), currMat->ambient[i]);
                }
                else if (prefix == "Kd")
                {
                    auto tokens = fixedTokenize<4>(line, " ");
                    if (currMat)
                        for (int i = 0; i < decltype(currMat->diffuse)::length(); ++i)
                            std::from_chars(tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), currMat->diffuse[i]);
                }
                else if (prefix == "Ks")
                {
                    auto tokens = fixedTokenize<4>(line, " ");
                    if (currMat)
                        for (int i = 0; i < decltype(currMat->specular)::length(); ++i)
                            std::from_chars(tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), currMat->specular[i]);
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
    }

    WavefrontObjReader::WavefrontObjReader(const std::filesystem::path& objFilePath)
    {
        read(objFilePath);
    }

    WavefrontObjReader::~WavefrontObjReader()
    {
    }

    bool WavefrontObjReader::read(const std::filesystem::path& objFilePath)
    {
        m_path = objFilePath;

        std::ifstream file(m_path);

        robin_hood::unordered_flat_map<ObjVertex, unsigned int, ObjVertexHasher> vertexMap;
        std::vector<glm::vec3> positionList;
        std::vector<glm::vec3> normalList;
        std::vector<glm::vec2> texCoordList;
        std::vector<glm::vec3> paramList;
        std::vector<MeshPart> materialList;
        robin_hood::unordered_flat_map<std::string, Material> materials;

        std::string line;
        unsigned int uniqueVertexId = 0;
        while (std::getline(file, line))
        {
            const std::size_t prefixEnd = line.find(' ', 0);
            const std::string_view prefix = std::string_view(line).substr(0, prefixEnd);

            if (prefix == "o")
            {
                const auto tokens = fixedTokenize<2>(line, " ");
                MeshPart meshPart;
                meshPart.tag = tokens[1];
                meshPart.offset = 3 * static_cast<uint32_t>(m_triangles.size());
                meshPart.count = 0;
                m_meshParts.push_back(meshPart);
            }
            else if (prefix == "mtllib")
            {
                const auto tokens = fixedTokenize<2>(line, " ");
                std::string_view materialFilename = tokens[1];
                std::filesystem::path materialFilePath = objFilePath.parent_path() / materialFilename;
                if (std::filesystem::exists(materialFilePath))
                    m_materials = loadMaterials(materialFilePath);
            }
            else if (prefix == "v")
            {
                glm::vec3 pos;

                const auto tokens = fixedTokenize<4>(line, " "); // v x y z, ignore w
                for (int i = 0; i < decltype(pos)::length(); ++i)
                    std::from_chars(tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), pos[i]);

                positionList.push_back(pos);
            }
            else if (prefix == "vt")
            {
                glm::vec2 texCoord;

                const auto tokens = fixedTokenize<3>(line, " ");
                for (int i = 0; i < glm::vec2::length(); ++i)
                    std::from_chars(tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), texCoord[i]);

                texCoordList.push_back(texCoord);
            }
            else if (prefix == "vn")
            {
                glm::vec3 normal;

                const auto tokens = fixedTokenize<4>(line, " ");
                for (int i = 0; i < glm::vec3::length(); ++i)
                    std::from_chars(tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), normal[i]);

                normalList.push_back(glm::normalize(normal));
            }
            else if (prefix == "vp")
            {
                glm::vec3 param;

                const auto tokens = fixedTokenize<4>(line, " ");
                for (int i = 0; i < glm::vec3::length(); ++i)
                    std::from_chars(tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), param[i]);

                paramList.push_back(param);
            }
            else if (prefix == "f")
            {
                glm::uvec3 faceIndices;

                const auto tokens = fixedTokenize<4>(line, " ");
                std::array<ObjVertex, 3> vertices;

                for (int i = 0; i < 3; i++)
                {
                    vertices[i] = ObjVertex(tokens[i + 1]);
                }

                for (int i = 0; i < 3; i++)
                {
                    const auto it = vertexMap.find(vertices[i]);
                    if (it == vertexMap.end())
                    {
                        vertexMap[vertices[i]] = uniqueVertexId;
                        faceIndices[i] = uniqueVertexId;
                        uniqueVertexId++;
                    }
                    else
                    {
                        faceIndices[i] = it->second;
                    }
                }

                m_triangles.push_back(faceIndices);
            }
            else if (prefix == "usemtl")
            {
                const auto tokens = fixedTokenize<2>(line, " ");
                MeshPart meshPart;
                meshPart.tag = tokens[1];
                meshPart.offset = 3 * static_cast<uint32_t>(m_triangles.size());
                meshPart.count = 0;
                m_meshParts.push_back(meshPart);
            }
            else if (prefix == "#")
            {
                // comment;
            }
        }

        m_triangles.shrink_to_fit();

        m_positions.resize(positionList.empty() ? 0 : vertexMap.size());
        m_normals.resize(normalList.empty() ? 0 : vertexMap.size());
        m_texCoords.resize(texCoordList.empty() ? 0 : vertexMap.size());

        for (const auto& item : vertexMap)
        {
            unsigned int vertexIdx = item.second;
            ObjVertex attribIndices = item.first;
            if (attribIndices.p < 0)
                attribIndices.p = positionList.size() + attribIndices.p;

            m_positions[vertexIdx] = positionList[attribIndices.p];

            if (attribIndices.n)
            {
                int64_t n = attribIndices.n.value();
                if (n < 0)
                    n = normalList.size() + n;
                m_normals[vertexIdx] = normalList[n];
            }


            if (attribIndices.uv)
            {
                int64_t uv = attribIndices.uv.value();
                if (uv < 0)
                    uv = texCoordList.size() + uv;
                m_texCoords[vertexIdx] = texCoordList[uv];
            }
        }

        if (m_meshParts.size() >= 1)
        {
            for (std::size_t i = 0; i < m_meshParts.size() - 1; ++i)
                m_meshParts[i].count = m_meshParts[i + 1].offset - m_meshParts[i].offset;

            m_meshParts.back().count = static_cast<uint32_t>(3 * m_triangles.size() - m_meshParts.back().offset);
        }

        m_meshParts.erase(std::remove_if(m_meshParts.begin(), m_meshParts.end(), [](const MeshPart& part) {
            return part.count == 0;
            }), m_meshParts.end());

        return true;
    }

    const std::filesystem::path& WavefrontObjReader::getPath() const
    {
        return m_path;
    }

    const std::vector<glm::vec3>& WavefrontObjReader::getPositions() const
    {
        return m_positions;
    }

    const std::vector<glm::vec3>& WavefrontObjReader::getNormals() const
    {
        return m_normals;
    }

    const std::vector<glm::vec2>& WavefrontObjReader::getTexCoords() const
    {
        return m_texCoords;
    }

    const std::vector<glm::uvec3>& WavefrontObjReader::getTriangles() const
    {
        return m_triangles;
    }

    bool WavefrontObjReader::isWavefrontObjFile(const std::filesystem::path& path)
    {
        const std::string ext = path.extension().string();
        return ext == ".obj";
    }
}
