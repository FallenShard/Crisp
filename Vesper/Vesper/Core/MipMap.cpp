#include "MipMap.hpp"

#include <filesystem>
#include <vector>
#include <iostream>
#include <algorithm>

#include <CrispCore/IO/OpenEXRReader.hpp>
#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
    MipMap::MipMap(std::filesystem::path filePath)
    {
        OpenEXRReader reader;
        std::vector<float> data;
        uint32_t width = 0;
        uint32_t height = 0;
        if (!reader.read(filePath, data, width, height))
        {
            width = 1;
            height = 1;
            data = { 1.0f, 1.0f, 1.0f };
        }

        m_texels.resize(m_resolution.y * m_resolution.x);
        for (uint32_t i = 0; i < height; ++i) {
            for (uint32_t j = 0; i < width; ++j) {
                const uint32_t idx = i * width + j;
                m_texels[idx].r = data[3 * idx + 0];
                m_texels[idx].g = data[3 * idx + 1];
                m_texels[idx].b = data[3 * idx + 2];
            }
        }

        m_resolution = glm::ivec2(width, height);
        m_invResolution = glm::vec2(1.0f / m_resolution.x, 1.0f / m_resolution.y);
    }

    int MipMap::getWidth() const
    {
        return m_resolution.x;
    }

    int MipMap::getHeight() const
    {
        return m_resolution.y;
    }

    Spectrum MipMap::fetch(int x, int y) const
    {
        x = std::max(0, std::min(x, m_resolution.x - 1));
        y = std::max(0, std::min(y, m_resolution.y - 1));

        return m_texels[y][x];
    }

    Spectrum MipMap::evalNN(float u, float v) const
    {
        auto x = std::max(0, std::min(static_cast<int>(std::round(u * m_resolution.x)), m_resolution.x - 1));
        auto y = std::max(0, std::min(static_cast<int>(std::round(v * m_resolution.y)), m_resolution.y - 1));

        return m_texels[y][x];
    }

    Spectrum MipMap::evalLerp(float u, float v) const
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
}