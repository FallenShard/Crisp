#include <CrispCore/Image/Image.hpp>

#include <cmath>

namespace crisp
{
    Image::Image(std::vector<uint8_t> pixelData, uint32_t width, uint32_t height, uint32_t channelCount, uint32_t pixelByteSize)
        : m_data(std::move(pixelData))
        , m_width(width)
        , m_height(height)
        , m_channelCount(channelCount)
        , m_pixelByteSize(pixelByteSize)
    {}

    uint32_t Image::getWidth() const
    {
        return m_width;
    }

    uint32_t Image::getHeight() const
    {
        return m_height;
    }

    uint32_t Image::getChannelCount() const
    {
        return m_channelCount;
    }

    uint32_t Image::getPixelByteSize() const
    {
        return m_pixelByteSize;
    }

    uint64_t Image::getByteSize() const
    {
        return m_data.size();
    }

    uint32_t Image::getMipLevels() const
    {
        return getMipLevels(m_width, m_height);
    }

    uint32_t Image::getMipLevels(uint32_t width, uint32_t height)
    {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    }

    Image Image::createDefaultAlbedoMap(const std::array<uint8_t, 4>& color)
    {
        return Image(std::vector<uint8_t>(color.begin(), color.end()), 1, 1, 4, 4 * sizeof(uint8_t));
    }

    Image Image::createDefaultNormalMap()
    {
        return Image(std::vector<uint8_t>{ 128, 128, 128, 255 }, 1, 1, 4, 4 * sizeof(uint8_t));
    }

    Image Image::createDefaultMetallicMap(const float metallic)
    {
        const uint8_t val = static_cast<uint8_t>(metallic * 255.0f);
        return Image(std::vector<uint8_t>{ val, val, val, 255 }, 1, 1, 4, 4 * sizeof(uint8_t));
    }

    Image Image::createDefaultRoughnessMap(const float roughness)
    {
        const uint8_t val = static_cast<uint8_t>(roughness * 255.0f);
        return Image(std::vector<uint8_t>{ val, val, val, 255 }, 1, 1, 4, 4 * sizeof(uint8_t));
    }

    Image Image::createDefaultAmbientOcclusionMap()
    {
        return Image(std::vector<uint8_t>{ 255, 255, 255, 255 }, 1, 1, 4, 4 * sizeof(uint8_t));
    }
}
