#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "Spectrums/Spectrum.hpp"

namespace vesper
{
    class ReconstructionFilter;

    class ImageBlock
    {
    public:
        struct Descriptor
        {
            glm::ivec2 offset;
            glm::ivec2 size;

            Descriptor(int x, int y, int width, int height);
            Descriptor();
        };

        ImageBlock();
        ImageBlock(const glm::ivec2& size, const ReconstructionFilter* filter);
        ImageBlock(const glm::ivec2& offset, const glm::ivec2& size, const ReconstructionFilter* filter);

        ~ImageBlock();

        void initialize(const glm::ivec2& size, const ReconstructionFilter* filter);
        void initialize(const glm::ivec2& offset, const glm::ivec2& size, const ReconstructionFilter* filter);
        void clear();

        glm::ivec2 getOffset() const;
        glm::ivec2 getSize() const;
        glm::ivec2 getFullSize() const;

        void addSample(const glm::vec2& pixelSample, const Spectrum& radiance);

        std::vector<float> getRaw();

    private:
        std::vector<std::vector<WeightedSpectrum>> m_pixels;
        glm::ivec2 m_offset;
        glm::ivec2 m_size;
        int m_borderSize;

        const ReconstructionFilter* m_filter;

        std::vector<std::vector<float>> m_weights;
    };
}