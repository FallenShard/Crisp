#include "FbxImporter.hpp"

#include "Geometry/TriangleMesh.hpp"

#include <fstream>
#include <array>
#include <iostream>
#include <streambuf>

#include <spdlog/spdlog.h>

#include <gzip/decompress.hpp>

namespace crisp
{
    namespace
    {
        uint32_t getTypeBytesize(char typeCode)
        {
            if (typeCode == 'S' || typeCode == 'R')
                return 1;
            if (typeCode == 'Y')
                return 2;
            if (typeCode == 'C')
                return 1;
            if (typeCode == 'I' || typeCode == 'F')
                return 4;
            if (typeCode == 'D' || typeCode == 'L')
                return 8;

            spdlog::error("Unknown typeCode {}", typeCode);
            return 0;
        }

        struct membuf : std::streambuf
        {
            membuf(char* begin, char* end) {
                this->setg(begin, begin, end);
            }
        };

        template <typename T>
        std::vector<T> decompress(const std::vector<char>& compressedBuffer, uint32_t elementCount)
        {
            std::string decompressed = gzip::decompress(compressedBuffer.data(), compressedBuffer.size());
            std::vector<T> result(elementCount);
            assert(decompressed.size() == elementCount * sizeof(T));
            std::memcpy(result.data(), decompressed.data(), decompressed.size());
            return result;
        }

        struct BinaryFile
        {
            std::vector<char> contents;
            std::size_t position;

            std::vector<double> positions;
            std::vector<double> normals;
            std::vector<double> uvs;
            std::vector<int> faces;
            std::vector<int> normalIndices;
            std::vector<int> uvIndices;

            template <typename T, int N>
            void read(std::array<T, N>& arr)
            {
                memcpy(arr.data(), &contents[position], N * sizeof(T));
                position += N * sizeof(T);
            }

            template <typename T>
            void read(std::vector<T>& vec)
            {
                memcpy(vec.data(), &contents[position], vec.size() * sizeof(T));
                position += vec.size() * sizeof(T);
            }

            void read(std::string& data)
            {
                memcpy(data.data(), &contents[position], data.size());
                position += data.size();
            }

            template <typename T>
            void read(T& data)
            {
                memcpy(reinterpret_cast<char*>(&data), &contents[position], sizeof(T));
                position += sizeof(T);
            }

            bool parseContent()
            {
                if (position >= contents.size())
                    return false;

                uint32_t endOffset, propertyCount, propertyListLength;
                uint8_t nameLength;
                read(endOffset);
                read(propertyCount);
                read(propertyListLength);
                read(nameLength);

                bool isNullRecord = endOffset == 0 && propertyCount == 0 && propertyListLength == 0 && nameLength == 0;
                if (isNullRecord)
                    return false;

                std::string name(nameLength, '\0');
                read(name);

                std::cout << "Node name: " << name;
                std::cout << "\nEnd offset: " << endOffset << '\n';
                std::cout << "Properties: " << propertyCount << " Length: " << propertyListLength << "\n";

                for (int i = 0; i < propertyCount; ++i)
                {
                    parseProperty(name);
                }

                uint32_t listLen = endOffset - position;
                if (listLen > 0)
                {
                    while (true)
                    {
                        bool found = parseContent();
                        if (!found)
                            break;
                    }
                }

                return true;
            }

            void parseProperty(const std::string& propertyName)
            {
                char typeCode;
                read(typeCode);
                std::cout << "Property type: " << typeCode << '\n';

                if (typeCode == 'S' || typeCode == 'R')
                {
                    uint32_t length;
                    read(length);

                    std::vector<char> buffer(length);
                    read(buffer);
                    if (typeCode == 'S')
                    {
                        for (auto c : buffer)
                            std::cout << c;
                        std::cout << '\n';
                    }
                }
                else if (typeCode == 'f' || typeCode == 'd' || typeCode == 'l' || typeCode == 'i' || typeCode == 'b')
                {
                    uint32_t byteSize = 1;
                    if (typeCode == 'f' || typeCode == 'i')
                        byteSize = 4;
                    else if (typeCode == 'd' || typeCode == 'l')
                        byteSize = 8;

                    uint32_t arrayLength, encoding, compressedLength;
                    read(arrayLength);
                    read(encoding);
                    read(compressedLength);

                    std::vector<char> buffer(compressedLength);
                    read(buffer);

                    if (encoding == 1)
                    {

                        if (byteSize == 8)
                        {
                            if (propertyName == "Vertices")
                                positions = decompress<double>(buffer, arrayLength);
                            else if (propertyName == "Normals")
                                normals = decompress<double>(buffer, arrayLength);
                            else if (propertyName == "UV")
                                uvs = decompress<double>(buffer, arrayLength);
                        }
                        else if (byteSize == 4)
                        {
                            if (propertyName == "PolygonVertexIndex")
                                faces = decompress<int>(buffer, arrayLength);
                            else if (propertyName == "NormalsIndex")
                                normalIndices = decompress<int>(buffer, arrayLength);
                            else if (propertyName == "UVIndex")
                                uvIndices = decompress<int>(buffer, arrayLength);
                        }
                    }

                    int x = 0;
                    ++x;
                }
                else
                {
                    uint32_t byteSize = 1;
                    switch (typeCode)
                    {
                    case 'F':
                        float f;
                        read(f);
                        std::cout << f << '\n';
                        break;
                    case 'I':
                        int i;
                        read(i);
                        std::cout << i << '\n';
                        break;
                    case 'D':
                        double d;
                        read(d);
                        std::cout << d << '\n';
                        break;
                    case 'L':
                        long long l;
                        read(l);
                        std::cout << l << '\n';
                        break;
                    case 'Y':
                        short int y;
                        read(y);
                        std::cout << y << '\n';
                        break;
                    case 'C':
                        bool b;
                        read(b);
                        std::cout << b << '\n';
                        break;
                    }
                }
            }
        };
    }

    FbxImporter::FbxImporter(const std::filesystem::path& filePath)
    {
        import(filePath);
    }

    FbxImporter::~FbxImporter()
    {
    }

    bool FbxImporter::import(const std::filesystem::path& filePath)
    {
        std::ifstream fileStream(filePath, std::ios::binary | std::ios::ate);
        std::size_t fileSize = fileStream.tellg();
        fileStream.seekg(0, std::ios::beg);

        BinaryFile file;
        file.contents.resize(fileSize);
        file.position = 0;
        fileStream.read(file.contents.data(), file.contents.size());


        std::array<char, 23> header;
        file.read(header);

        uint32_t version;
        file.read(version);

        while (true)
        {
            bool parsed = file.parseContent();
            if (!parsed)
                break;
        }

        m_positions.resize(file.positions.size() / 3);
        m_normals.resize(file.normals.size() / 3);
        m_texCoords.resize(file.uvs.size() / 2);

        for (std::size_t i = 0; i < m_positions.size(); ++i)
        {
            m_positions[i] = glm::vec3(file.positions[3 * i], file.positions[3 * i + 1], file.positions[3 * i + 2]);
            m_normals[i] = glm::vec3(file.positions[3 * i], file.positions[3 * i + 1], file.positions[3 * i + 2]);
            m_texCoords[i] = glm::vec2(file.positions[2 * i], file.positions[2 * i + 1]);
        }

        m_triangles.resize(file.faces.size() / 3);
        for (std::size_t i = 0; i < m_triangles.size(); ++i)
        {
            int v0 = file.faces[3 * i];
            int v1 = file.faces[3 * i + 1];
            int v2 = file.faces[3 * i + 2];
            m_triangles[i] = glm::uvec3(v0, v1, ~v2);
        }

        m_meshParts.push_back({ "", 0, static_cast<uint32_t>(m_triangles.size()) });


        return true;
    }

    std::unique_ptr<TriangleMesh> FbxImporter::convertToTriangleMesh()
    {
        return nullptr;
    }

    bool FbxImporter::isFbxFile(const std::filesystem::path& filePath)
    {
        return filePath.extension().string() == ".fbx";
    }

    void FbxImporter::moveDataToTriangleMesh(TriangleMesh& triangleMesh, std::vector<VertexAttributeDescriptor> vertexAttribs)
    {
        triangleMesh.setMeshName(m_path.filename().stem().string());
        triangleMesh.setFaces(std::move(m_triangles));

        bool computeVertexNormals = false;
        bool computeTangentVectors = false;
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
        triangleMesh.computeBoundingBox();
    }
}


