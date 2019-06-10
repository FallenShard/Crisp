#include "WavefrontObjImporter.hpp"

#include "Geometry/TriangleMesh.hpp"

#include <iostream>
#include <charconv>
#include <array>
#include <string_view>
#include <fstream>
#include <string>
#include <unordered_set>

namespace crisp
{
    namespace
    {
        template <std::size_t MaxNumTokens>
        std::array<std::string_view, MaxNumTokens> fixedTokenize(std::string_view string, std::string_view delimiter)
        {
            std::array<std::string_view, MaxNumTokens> result;
            size_t start = 0;
            size_t end = 0;

            size_t idx = 0;

            while (start < string.size())
            {
                end = string.find_first_of(delimiter, start);

                if (start != end && idx < MaxNumTokens)
                {
                    result[idx] = string.substr(start, end - start);
                    ++idx;
                }

                if (end == std::string_view::npos)
                    break;

                start = end + 1;
            }

            return result;
        }

        struct ObjVertex
        {
            int p = -1;
            int n = -1;
            int uv = -1;

            inline ObjVertex() {};
            inline ObjVertex(std::string_view stringView)
            {
                auto vertexAttribs = fixedTokenize<3>(stringView, "/");

                auto parse = [](const std::string_view & line_view, int& val)
                {
                    std::from_chars(line_view.data(), line_view.data() + line_view.size(), val);
                    --val; // OBJ face vertex indices start with 1
                };

                if (vertexAttribs.size() > 0)
                    parse(vertexAttribs[0], p);

                if (vertexAttribs.size() > 1 && !vertexAttribs[1].empty())
                    parse(vertexAttribs[1], uv);

                if (vertexAttribs.size() > 2 && !vertexAttribs[2].empty())
                    parse(vertexAttribs[2], n);
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
                size_t hash = std::hash<int>()(v.p);
                hash = hash * 37 + std::hash<int>()(v.uv);
                hash = hash * 37 + std::hash<int>()(v.n);
                return hash;
            }
        };
    }

    WavefrontObjImporter::WavefrontObjImporter(const std::filesystem::path& objFilePath)
    {
        import(objFilePath);
    }

    WavefrontObjImporter::~WavefrontObjImporter()
    {
    }

    bool WavefrontObjImporter::import(const std::filesystem::path& objFilePath)
    {
        m_path = objFilePath;

        std::ifstream file(m_path);

        std::unordered_map<ObjVertex, unsigned int, ObjVertexHasher> vertexMap;
        std::vector<glm::vec3> tempPositions;
        std::vector<glm::vec3> tempNormals;
        std::vector<glm::vec2> tempTexCoords;
        std::string line;
        unsigned int uniqueVertexId = 0;
        while (std::getline(file, line))
        {
            std::size_t prefixEnd = line.find_first_of(' ', 0);
            std::string_view prefix = std::string_view(line).substr(0, prefixEnd);

            if (prefix == "o")
            {
                auto tokens = fixedTokenize<2>(line, " ");
                MeshPart meshPart;
                meshPart.tag = tokens[1];
                meshPart.offset = 3 * static_cast<uint32_t>(m_triangles.size());
                meshPart.count  = 0;
                m_meshParts.push_back(meshPart);
            }
            else if (prefix == "v")
            {
                glm::vec3 pos;

                auto tokens = fixedTokenize<4>(line, " ");
                for (int i = 0; i < glm::vec3::length(); ++i)
                    std::from_chars(tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), pos[i]);

                tempPositions.push_back(pos);
            }
            else if (prefix == "vt")
            {
                glm::vec2 texCoord;

                auto tokens = fixedTokenize<3>(line, " ");
                for (int i = 0; i < glm::vec2::length(); ++i)
                    std::from_chars(tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), texCoord[i]);

                tempTexCoords.push_back(texCoord);
            }
            else if (prefix == "vn")
            {
                glm::vec3 normal;

                auto tokens = fixedTokenize<4>(line, " ");
                for (int i = 0; i < glm::vec3::length(); ++i)
                    std::from_chars(tokens[i + 1].data(), tokens[i + 1].data() + tokens[i + 1].size(), normal[i]);

                tempNormals.push_back(glm::normalize(normal));
            }
            else if (prefix == "f")
            {
                glm::uvec3 faceIndices;

                auto tokens = fixedTokenize<4>(line, " ");
                std::array<ObjVertex, 3> vertices;

                for (int i = 0; i < 3; i++)
                {
                    vertices[i] = ObjVertex(tokens[i + 1]);
                }

                for (int i = 0; i < 3; i++)
                {
                    auto it = vertexMap.find(vertices[i]);
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
        }

        m_triangles.shrink_to_fit();

        m_positions.resize(tempPositions.empty() ? 0 : vertexMap.size());
        m_normals.resize(tempNormals.empty() ? 0 : vertexMap.size());
        m_texCoords.resize(tempTexCoords.empty() ? 0 : vertexMap.size());

        for (const auto& item : vertexMap)
        {
            unsigned int vertexIdx = item.second;
            ObjVertex attribIndices = item.first;

            m_positions[vertexIdx] = tempPositions[attribIndices.p];

            if (attribIndices.n != -1)
                m_normals[vertexIdx] = tempNormals[attribIndices.n];

            if (attribIndices.uv != -1)
                m_texCoords[vertexIdx] = tempTexCoords[attribIndices.uv];
        }

        if (m_meshParts.size() >= 1)
        {
            for (std::size_t i = 0; i < m_meshParts.size() - 1; ++i)
                m_meshParts[i].count = m_meshParts[i + 1].offset - m_meshParts[i].offset;

            m_meshParts.back().count = static_cast<uint32_t>(3 * m_triangles.size() - m_meshParts.back().offset);
        }

        return true;
    }

    std::unique_ptr<TriangleMesh> WavefrontObjImporter::convertToTriangleMesh()
    {
        std::vector<VertexAttributeDescriptor> vertexAttribs = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord };
        std::unique_ptr<TriangleMesh> mesh = std::make_unique<TriangleMesh>();
        moveDataToTriangleMesh(*mesh, vertexAttribs);
        return std::move(mesh);
    }

    void WavefrontObjImporter::moveDataToTriangleMesh(TriangleMesh& triangleMesh, std::vector<VertexAttributeDescriptor> vertexAttribs)
    {
        triangleMesh.setMeshName(m_path.filename().stem().string());
        triangleMesh.setFaces(std::move(m_triangles));

        bool computeVertexNormals   = false;
        bool computeTangentVectors  = false;
        for (const auto& attrib : vertexAttribs)
        {
            if (attrib.type == VertexAttribute::Position)
            {
                triangleMesh.setPositions(std::move(m_positions));
            }
            else if (attrib.type == VertexAttribute::Normal)
            {
                if (m_normals.empty())
                    computeVertexNormals = true;
                else
                    triangleMesh.setNormals(std::move(m_normals));
            }
            else if (attrib.type == VertexAttribute::TexCoord)
            {
                if (m_texCoords.empty())
                {
                    for (uint32_t i = 0; i < triangleMesh.getNumVertices(); ++i)
                        m_texCoords.push_back(glm::vec2(0.0));
                }

                triangleMesh.setTexCoords(std::move(m_texCoords));
            }
            else if (attrib.type == VertexAttribute::Tangent || attrib.type == VertexAttribute::Bitangent)
            {
                computeTangentVectors = true;
            }
        }

        if (computeVertexNormals)
            triangleMesh.computeVertexNormals();

        if (computeTangentVectors)
            triangleMesh.computeTangentVectors();

        std::vector<GeometryPart> parts;
        for (const auto& part : m_meshParts)
            parts.emplace_back(part.tag, part.offset, part.count);

        triangleMesh.setParts(std::move(parts));
    }

    const std::filesystem::path& WavefrontObjImporter::getPath() const
    {
        return m_path;
    }

    bool WavefrontObjImporter::isWavefrontObjFile(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        return ext == ".obj";
    }
}


