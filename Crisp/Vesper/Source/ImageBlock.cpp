#include "ImageBlock.hpp"
#include "ReconstructionFilters/ReconstructionFilter.hpp"

#include <iostream>

namespace vesper
{
    ImageBlock::ImageBlock()
    {
    }

    ImageBlock::ImageBlock(const glm::ivec2& size, const ReconstructionFilter* filter)
        : m_offset(0, 0)
    {
        initialize(size, filter);
    }

    ImageBlock::ImageBlock(const glm::ivec2& offset, const glm::ivec2& size, const ReconstructionFilter* filter)
    {
        initialize(offset, size, filter);
    }

    ImageBlock::~ImageBlock()
    {
    }

    void ImageBlock::initialize(const glm::ivec2& size, const ReconstructionFilter* filter)
    {
        m_size = size;
        if (filter)
        {
            m_filter = filter;
            m_borderSize = static_cast<int>(std::ceil(m_filter->getRadius() - 0.5f));
            int wSize = static_cast<int>(std::ceil(2.0f * m_filter->getRadius())) + 1;
            m_weights.resize(wSize);
            for (auto& weightRow : m_weights)
            {
                weightRow.resize(wSize);
                for (auto& weight : weightRow)
                    weight = 0.0f;
            }
        }
        else
        {
            m_borderSize = 0;
            m_filter = nullptr;
        }

        m_pixels.clear();
        m_pixels.resize(size.y + 2 * m_borderSize);
        for (auto& pixelRow : m_pixels)
            pixelRow.resize(size.x + 2 * m_borderSize);
    }

    void ImageBlock::initialize(const glm::ivec2& offset, const glm::ivec2& size, const ReconstructionFilter* filter)
    {
        m_offset = offset;
        initialize(size, filter);
    }

    void ImageBlock::clear()
    {
        for (auto& pixelRow : m_pixels)
            for (auto& pixel : pixelRow)
                pixel = WeightedSpectrum(0.0f);
    }

    glm::ivec2 ImageBlock::getOffset() const
    {
        return m_offset;
    }

    glm::ivec2 ImageBlock::getSize() const
    {
        return m_size;
    }

    glm::ivec2 ImageBlock::getFullSize() const
    {
        return m_size + glm::ivec2(m_borderSize, m_borderSize);
    }

    void ImageBlock::addSample(const glm::vec2& pixelSample, const Spectrum& radiance)
    {
        if (!radiance.isValid())
        {
            std::cerr << "Integrator computed an invalid radiance value!\n";
            return;
        }

        glm::vec2 pos(pixelSample.x - 0.5f - (m_offset.x - m_borderSize),
                      pixelSample.y - 0.5f - (m_offset.y - m_borderSize));

        int xLo = std::max(0, static_cast<int>(std::ceil(pos.x - m_filter->getRadius())));
        int yLo = std::max(0, static_cast<int>(std::ceil(pos.y - m_filter->getRadius())));
        int xHi = std::min(static_cast<int>(m_pixels.at(0).size() - 1), static_cast<int>(std::floor(pos.x + m_filter->getRadius())));
        int yHi = std::min(static_cast<int>(m_pixels.size()       - 1), static_cast<int>(std::floor(pos.y + m_filter->getRadius())));

        for (int y = yLo, wy = 0; y <= yHi; ++y, ++wy)
            for (int x = xLo, wx = 0; x <= xHi; ++x, ++wx)
                m_weights[wy][wx] = m_filter->evalDiscrete(x - pos.x, y - pos.y);

        for (int y = yLo, wy = 0; y <= yHi; ++y, ++wy)
            for (int x = xLo, wx = 0; x <= xHi; ++x, ++wx)
                m_pixels[y][x] += WeightedSpectrum(radiance) * m_weights[wy][wx];
    }

    std::vector<float> ImageBlock::getRaw()
    {
        std::vector<float> data;
        data.reserve(m_size.x * m_size.y * 3);

        for (int i = m_borderSize; i < m_borderSize + m_size.y; i++)
        {
            for (int j = m_borderSize; j < m_borderSize + m_size.x; j++)
            {
                auto rgb = m_pixels[i][j].toRgb();
                data.push_back(rgb.r);
                data.push_back(rgb.g);
                data.push_back(rgb.b);
            }
        }

        return data;
    }

    ImageBlock::Descriptor::Descriptor(int xOffset, int yOffset, int width, int height)
    {
        offset = glm::ivec2(xOffset, yOffset);
        size   = glm::ivec2(width, height);
    }

    ImageBlock::Descriptor::Descriptor()
    {
        offset = glm::ivec2(0, 0);
        size   = glm::ivec2(0, 0);
    }
}