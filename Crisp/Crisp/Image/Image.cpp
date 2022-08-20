#include <Crisp/Image/Image.hpp>

#include <cmath>

namespace crisp
{
Image::Image(
    std::vector<uint8_t> pixelData, uint32_t width, uint32_t height, uint32_t channelCount, uint32_t pixelByteSize)
    : m_data(std::move(pixelData))
    , m_width(width)
    , m_height(height)
    , m_channelCount(channelCount)
    , m_pixelByteSize(pixelByteSize)
{
}

Image Image::createFromChannel(uint32_t channelIndex) const
{
    constexpr uint32_t dstChannelCount = 1;
    const uint32_t channelByteSize{m_pixelByteSize / m_channelCount};
    std::vector<uint8_t> pixelData(m_width * m_height * dstChannelCount * channelByteSize);
    for (uint32_t i = 0; i < m_height; ++i)
    {
        for (uint32_t j = 0; j < m_width; ++j)
        {
            const uint8_t* srcStart = &m_data.at((i * m_width + j) * m_pixelByteSize + channelByteSize * channelIndex);
            uint8_t* dstStart = &pixelData.at((i * m_width + j) * channelByteSize);
            std::memcpy(dstStart, srcStart, channelByteSize);
        }
    }

    return Image(std::move(pixelData), m_width, m_height, dstChannelCount, channelByteSize);
}

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

} // namespace crisp
