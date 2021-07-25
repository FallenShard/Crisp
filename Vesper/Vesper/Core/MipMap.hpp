#pragma once

#include "Spectrums/Spectrum.hpp"

#include <CrispCore/Math/Headers.hpp>

#include <filesystem>
#include <vector>


namespace crisp
{
    class MipMap
    {
    public:
        MipMap(std::filesystem::path filePath);

        int getWidth() const;
        int getHeight() const;

        Spectrum fetch(int x, int y) const;

        Spectrum evalNN(float u, float v) const;
        Spectrum evalLerp(float u, float v) const;

    private:
        glm::ivec2 m_resolution;
        glm::vec2 m_invResolution;
        std::vector<Spectrum> m_texels;
    };
}