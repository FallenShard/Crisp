#include "MeshLoader.hpp"

#include <unordered_map>
#include <iostream>
#include <sstream>

#include <glm/glm.hpp>

#include "Math/Transform.hpp"

namespace vesper
{
    namespace
    {
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
    }

    bool MeshLoader::load(std::string fileName, 
        std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals, 
        std::vector<glm::vec2>& texCoords, std::vector<glm::uvec3>& faces,
        const Transform& transform) const
    {
        std::string meshFilename = "Resources/Meshes/" + fileName;
        std::ifstream meshFile(meshFilename);
        if (meshFile.fail())
            return false;

        auto tokens = tokenize(meshFilename, ".");
        std::string ext = tokens[tokens.size() - 1];

        if (ext == "obj")
        {
            std::cout << "Loading Wavefront Obj mesh: " + meshFilename << std::endl;
            loadWavefrontObj(meshFile, positions, normals, texCoords, faces, transform);
            return true;
        }


        return false;
    }

    void MeshLoader::loadWavefrontObj(std::ifstream& file, 
        std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals, 
        std::vector<glm::vec2>& texCoords, std::vector<glm::uvec3>& faces,
        const Transform& transform) const
    {
        using VertexMap = std::unordered_map<ObjVertex, unsigned int, ObjVertexHasher>;

        VertexMap vertexMap;

        std::vector<glm::vec3> tempPositions;
        std::vector<glm::vec3> tempNormals;
        std::vector<glm::vec2> tempTexCoords;
        std::string line;
        unsigned int uniqueVertexId = 0;
        while (std::getline(file, line))
        {
            std::istringstream lineStream(line);

            std::string prefix;
            lineStream >> prefix;

            if (prefix == "v")
            {
                glm::vec3 p;
                lineStream >> p.x >> p.y >> p.z;
                tempPositions.push_back(transform.transformPoint(p));
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
                tempNormals.push_back(transform.transformNormal(glm::normalize(n)));
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

                faces.push_back(glm::uvec3(indices[0], indices[1], indices[2]));
            }
        }

        faces.shrink_to_fit();

        positions.resize(tempPositions.empty() ? 0 : vertexMap.size());
        normals.resize(tempNormals.empty() ? 0 : vertexMap.size());
        texCoords.resize(tempTexCoords.empty() ? 0 : vertexMap.size());

        for (auto& vertex : vertexMap)
        {
            auto vertexId = vertex.second;
            auto attribIndices = vertex.first;
            positions[vertexId] = tempPositions[attribIndices.p];
            if (attribIndices.n != -1)
                normals[vertexId] = tempNormals[attribIndices.n];
            if (attribIndices.uv != -1)
                texCoords[vertexId] = tempTexCoords[attribIndices.uv];
        }
    }

}