#include "Geometry.hpp"

#include <fstream>
#include <sstream>

#include <CrispCore/Log.hpp>
#include "Geometry/TriangleMesh.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    Geometry::Geometry(Renderer* renderer, const TriangleMesh& triangleMesh)
        : Geometry(renderer, triangleMesh.interleaveAttributes(), triangleMesh.getFaces())
    {
        m_vertexCount = triangleMesh.getNumVertices();
        m_indexCount  = triangleMesh.getNumIndices();
    }

    void Geometry::addVertexBuffer(std::unique_ptr<VulkanBuffer> vertexBuffer)
    {
        m_vertexBuffers.push_back(std::move(vertexBuffer));
        m_buffers.push_back(m_vertexBuffers.back()->getHandle());
        m_offsets.push_back(0);
        m_bindingCount = static_cast<uint32_t>(m_buffers.size());
    }

    void Geometry::bind(VkCommandBuffer commandBuffer) const
    {
        vkCmdBindVertexBuffers(commandBuffer, m_firstBinding, m_bindingCount, m_buffers.data(), m_offsets.data());
        if (m_indexBuffer)
            vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getHandle(), 0, VK_INDEX_TYPE_UINT32);
    }

    void Geometry::draw(VkCommandBuffer commandBuffer) const
    {
        if (m_indexBuffer)
            vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
        else
            vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
    }

    void Geometry::bindAndDraw(VkCommandBuffer commandBuffer) const
    {
        vkCmdBindVertexBuffers(commandBuffer, m_firstBinding, m_bindingCount, m_buffers.data(), m_offsets.data());
        if (m_indexBuffer)
        {
            vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getHandle(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
        }
    }

    void Geometry::bindVertexBuffers(VkCommandBuffer cmdBuffer) const
    {
        vkCmdBindVertexBuffers(cmdBuffer, m_firstBinding, m_bindingCount, m_buffers.data(), m_offsets.data());
    }

    IndexedGeometryView Geometry::createIndexedGeometryView() const
    {
        return { m_indexBuffer->getHandle(), m_indexCount, m_instanceCount, 0, 0, 0 };
    }

    std::unique_ptr<Geometry> createGeometry(Renderer* renderer, std::filesystem::path&& path, std::initializer_list<VertexAttribute> vertexAttributes)
    {
        return std::make_unique<Geometry>(renderer, TriangleMesh(std::move(path), vertexAttributes));
    }

    std::unique_ptr<Geometry> createGeometry(Renderer* renderer, TriangleMesh&& triangleMesh)
    {
        return std::make_unique<Geometry>(renderer, triangleMesh);
    }

    struct MeshData
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        std::vector<glm::uvec3> faces;
    };

    static std::vector<std::string> tokenize(const std::string& string, const std::string& delimiter)
    {
        std::vector<std::string> result;
        size_t start = 0;
        size_t end = 0;

        while (end != std::string::npos)
        {
            end = string.find(delimiter, start);

            result.push_back(string.substr(start, (end == std::string::npos) ? std::string::npos : end - start));

            start = ((end > (std::string::npos - delimiter.size())) ? std::string::npos : end + delimiter.size());
        }

        return result;
    }

    struct ObjVertex
    {
        int p = -1;
        int n = -1;
        int uv = -1;

        inline ObjVertex() {};
        inline ObjVertex(const std::string& str)
        {
            auto vertexAttribs = tokenize(str, "/");

            if (vertexAttribs.size() > 0)
            {
                std::stringstream posAttrib(vertexAttribs[0]);
                posAttrib >> p;
                p--;
            }

            if (vertexAttribs.size() > 1 && vertexAttribs[1] != "")
            {
                std::stringstream texCoordAttrib(vertexAttribs[1]);
                texCoordAttrib >> uv;
                uv--;
            }

            if (vertexAttribs.size() > 2 && vertexAttribs[2] != "")
            {
                std::stringstream normalAttrib(vertexAttribs[2]);
                normalAttrib >> n;
                n--;
            }
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

    std::pair<std::unique_ptr<Geometry>, std::vector<std::pair<uint32_t, uint32_t>>> loadMesh(Renderer* renderer, const std::string& filename, std::initializer_list<VertexAttribute> vertexAttributes)
    {
        std::ifstream file(renderer->getResourcesPath() / "Meshes" / filename);
        using VertexMap = std::unordered_map<ObjVertex, unsigned int, ObjVertexHasher>;

        VertexMap vertexMap;
        std::vector<glm::vec3> tempPositions;
        std::vector<glm::vec3> tempNormals;
        std::vector<glm::vec2> tempTexCoords;
        std::vector<glm::uvec3> tempFaces;

        std::vector<std::pair<uint32_t, uint32_t>> partOffsets;

        std::string line;
        unsigned int uniqueVertexId = 0;
        unsigned int lastPartFaceIndex = 0;
        while (std::getline(file, line))
        {
            std::istringstream lineStream(line);

            std::string prefix;
            lineStream >> prefix;

            if (prefix == "o")
            {
                std::string objectName;
                lineStream >> objectName;
                logInfo("Parsing object: ", objectName);
                partOffsets.push_back(std::make_pair(3 * static_cast<uint32_t>(tempFaces.size()), 0));
            }
            else if (prefix == "v")
            {
                glm::vec3 p;
                lineStream >> p.x >> p.y >> p.z;
                tempPositions.push_back(p);
            }
            else if (prefix == "vt")
            {
                glm::vec2 tc;
                lineStream >> tc.x >> tc.y;
                tempTexCoords.push_back(tc);
            }
            else if (prefix == "vn")
            {
                glm::vec3 n;
                lineStream >> n.x >> n.y >> n.z;
                tempNormals.push_back(glm::normalize(n));
            }
            else if (prefix == "f")
            {
                std::string vertexStrings[3];
                ObjVertex vertices[3];

                for (int i = 0; i < 3; i++)
                {
                    lineStream >> vertexStrings[i];
                    vertices[i] = ObjVertex(vertexStrings[i]);
                }

                unsigned int indices[3];
                for (int i = 0; i < 3; i++)
                {
                    auto it = vertexMap.find(vertices[i]);
                    if (it == vertexMap.end())
                    {
                        vertexMap[vertices[i]] = static_cast<unsigned int>(uniqueVertexId);
                        indices[i] = static_cast<unsigned int>(uniqueVertexId);
                        uniqueVertexId++;
                    }
                    else
                    {
                        indices[i] = it->second;
                    }
                }

                tempFaces.push_back(glm::uvec3(indices[0], indices[1], indices[2]));
            }
        }

        for (int i = 0; i < partOffsets.size() - 1; i++)
            partOffsets[i].second = partOffsets[i + 1].first - partOffsets[i].first;
        partOffsets.back().second = static_cast<uint32_t>(3 * tempFaces.size() - partOffsets.back().first);

        return std::make_pair(std::make_unique<Geometry>(renderer, TriangleMesh(filename, vertexAttributes)), partOffsets);
    }
}
