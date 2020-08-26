#pragma once

#include <filesystem>
#include <vector>
#include <iostream>
#include <algorithm>

#include <tinyexr/tinyexr.h>
#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
    template <typename T>
    class MipMap
    {
    public:
        MipMap(std::filesystem::path filePath)
        {
            if (!loadExrImage(filePath))
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
            x = std::max(0, std::min(x, m_resolution.x - 1));
            y = std::max(0, std::min(y, m_resolution.y - 1));

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
            auto fullU = u * m_resolution.x - 0.5f;
            auto fullV = v * m_resolution.y - 0.5f;

            auto u0 = std::max(0, std::min(static_cast<int>(std::floorf(fullU)), m_resolution.x - 1));
            auto u1 = std::max(0, std::min(u0 + 1, m_resolution.x - 1));
            auto v0 = std::max(0, std::min(static_cast<int>(std::floorf(fullV)), m_resolution.y - 1));
            auto v1 = std::max(0, std::min(v0 + 1, m_resolution.y - 1));

            auto du = fullU - u0;
            auto dv = fullV - v0;

            auto a0 = m_texels[v0][u0] * (1.0f - du) + m_texels[v0][u1] * du;
            auto a1 = m_texels[v1][u0] * (1.0f - du) + m_texels[v1][u1] * du;

            return a0 * (1.0f - dv) + a1 * dv;
        }

    private:
        bool loadExrImage(std::filesystem::path path)
        {
            auto pathString = path.string();

            // Read EXR version
            EXRVersion version;
            int retCode = ParseEXRVersionFromFile(&version, pathString.c_str());
            if (retCode != 0)
            {
                std::cerr << "Invalid EXR file: " << pathString << std::endl;
                return false;
            }

            // Make sure it is not multipart
            if (version.multipart)
            {
                std::cerr << "Multipart EXR files are not supported: " << pathString << std::endl;
                return false;
            }

            // Read EXR header
            EXRHeader header;
            InitEXRHeader(&header);

            const char* err;
            retCode = ParseEXRHeaderFromFile(&header, &version, pathString.c_str(), &err);
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
            retCode = LoadEXRImageFromFile(&image, &header, pathString.c_str(), &err);
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