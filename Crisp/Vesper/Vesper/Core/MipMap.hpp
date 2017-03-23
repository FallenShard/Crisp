#pragma once

#include <vector>
#include <iostream>
#include <algorithm>

#include <tinyexr/tinyexr.h>
#include <glm/glm.hpp>

namespace vesper
{
    template <typename T>
    class MipMap
    {
    public:
        MipMap(std::string fileName)
        {
            if (!loadExrImage(fileName))
            {
                m_resolution = glm::ivec2(1);

                m_texels.resize(m_resolution.y);
                m_texels[0].resize(m_resolution.x);
                m_texels[0][0] = T(1.0f);
            }

            m_resolution = glm::ivec2(m_texels[0].size(), m_texels.size());
            m_invResolution = glm::vec2(1.0f / m_resolution.x, 1.0f / m_resolution.y);
        }

        int getWidth() const
        {
            return m_resolution.x;
        }

        int getHeight() const
        {
            return m_resolution.y;
        }

        T fetch(int x, int y) const
        {
            return m_texels[y][x];
        }

        T evalNN(float u, float v) const
        {
            auto x = std::max(0, std::min(static_cast<int>(std::round(u * m_resolution.x)), m_resolution.x - 1));
            auto y = std::max(0, std::min(static_cast<int>(std::round(v * m_resolution.y)), m_resolution.y - 1));

            return m_texels[y][x];
        }

        T evalLerp(float u, float v) const
        {
            auto rU = u * m_resolution.x;
            auto rV = v * m_resolution.y;

            auto fU = std::max(0, std::min(static_cast<int>(std::floorf(rU)), m_resolution.x - 1));
            auto cU = std::max(0, std::min(static_cast<int>(std::ceilf(rU)), m_resolution.x - 1));
            auto fV = std::max(0, std::min(static_cast<int>(std::floorf(rV)), m_resolution.y - 1));
            auto cV = std::max(0, std::min(static_cast<int>(std::ceilf(rV)), m_resolution.y - 1));

            auto tU = rU - fU;
            auto tV = rV - fV;

            auto a0 = m_texels[fV][fU] * (1.0f - tU) + m_texels[fV][cU] * tU;
            auto a1 = m_texels[cV][fU] * (1.0f - tU) + m_texels[cV][cU] * tU;

            return a0 * (1 - tV) + a1 * tV;
        }

    private:
        bool loadExrImage(std::string fileName)
        {
            auto filePath = "Resources/Textures/" + fileName;

            // Read EXR version
            EXRVersion version;
            int retCode = ParseEXRVersionFromFile(&version, filePath.c_str());
            if (retCode != 0)
            {
                std::cerr << "Invalid EXR file: " << filePath << std::endl;
                return false;
            }

            // Make sure it is not multipart
            if (version.multipart)
            {
                std::cerr << "Multipart EXR files are not supported: " << filePath << std::endl;
                return false;
            }

            // Read EXR header
            EXRHeader header;
            InitEXRHeader(&header);

            const char* err;
            retCode = ParseEXRHeaderFromFile(&header, &version, filePath.c_str(), &err);
            if (retCode != 0)
            {
                std::cerr << "Parse EXR error: " << err << std::endl;
                return false;
            }

            // Request half-floats to be converted to 32bit floats
            for (int i = 0; i < header.num_channels; i++)
            {
                if (header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF)
                {
                    header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
                }
            }

            EXRImage image;
            InitEXRImage(&image);

            // Load image
            retCode = LoadEXRImageFromFile(&image, &header, filePath.c_str(), &err);
            if (retCode != 0)
            {
                std::cerr << "Load EXR image error: " << err << std::endl;
                return false;
            }

            m_texels.resize(image.height);
            for (int i = 0; i < image.height; i++)
            {
                m_texels[i].resize(image.width);
                for (int j = 0; j < image.width; j++)
                {
                    int currIdx = image.width * i + j;

                    m_texels[i][j].b = std::max(0.0f, reinterpret_cast<float**>(image.images)[0][currIdx]);
                    m_texels[i][j].g = std::max(0.0f, reinterpret_cast<float**>(image.images)[1][currIdx]);
                    m_texels[i][j].r = std::max(0.0f, reinterpret_cast<float**>(image.images)[2][currIdx]);
                }
            }

            FreeEXRImage(&image);

            return true;
        }

        glm::ivec2 m_resolution;
        glm::vec2 m_invResolution;
        std::vector<std::vector<T>> m_texels;
    };
}