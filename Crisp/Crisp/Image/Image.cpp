#include <Crisp/Image/Image.hpp>

#include <cmath>

namespace crisp
{
Image::Image()
    : m_width(0)
    , m_height(0)
    , m_channelCount(0)
    , m_pixelByteSize(0)
{
}

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
    constexpr uint32_t kDstChannelCount = 1;
    const uint32_t channelByteSize{m_pixelByteSize / m_channelCount};
    std::vector<uint8_t> pixelData(m_width * m_height * kDstChannelCount * channelByteSize);
    for (uint32_t i = 0; i < m_height; ++i)
    {
        for (uint32_t j = 0; j < m_width; ++j)
        {
            const uint8_t* srcStart = &m_data.at((i * m_width + j) * m_pixelByteSize + channelByteSize * channelIndex);
            uint8_t* dstStart = &pixelData.at((i * m_width + j) * channelByteSize);
            std::memcpy(dstStart, srcStart, channelByteSize);
        }
    }

    return Image(std::move(pixelData), m_width, m_height, kDstChannelCount, channelByteSize);
}

Image Image::createSubImage(uint32_t row, uint32_t col, uint32_t width, uint32_t height) const
{
    std::vector<uint8_t> subImagePixelData(width * height * m_pixelByteSize);
    for (uint32_t i = row; i < row + height; ++i)
    {
        for (uint32_t j = col; j < col + width; ++j)
        {
            const uint8_t* srcStart = &m_data.at((i * m_width + j) * m_pixelByteSize);
            uint8_t* dstStart = &subImagePixelData.at(((i - row) * width + j - col) * m_pixelByteSize);
            std::memcpy(dstStart, srcStart, m_pixelByteSize);
        }
    }
    return Image(std::move(subImagePixelData), width, height, m_channelCount, m_pixelByteSize);
}

void Image::transpose()
{
    for (uint32_t i = 0; i < m_height; ++i)
    {
        for (uint32_t j = i + 1; j < m_width; ++j)
        {
            for (uint32_t k = 0; k < m_pixelByteSize; ++k)
            {
                std::swap(
                    m_data.at((i * m_width + j) * m_pixelByteSize + k),
                    m_data.at((j * m_width + i) * m_pixelByteSize + k));
            }
        }
    }
}

void Image::mirrorX()
{
    for (uint32_t i = 0; i < m_height; ++i)
    {
        for (uint32_t j = 0; j < m_width / 2; ++j)
        {
            for (uint32_t k = 0; k < m_pixelByteSize; ++k)
            {
                std::swap(
                    m_data.at((i * m_width + j) * m_pixelByteSize + k),
                    m_data.at((i * m_width + m_width - 1 - j) * m_pixelByteSize + k));
            }
        }
    }
}

void Image::mirrorY()
{
    for (uint32_t i = 0; i < m_height / 2; ++i)
    {
        for (uint32_t j = 0; j < m_width; ++j)
        {
            for (uint32_t k = 0; k < m_pixelByteSize; ++k)
            {
                std::swap(
                    m_data.at((i * m_width + j) * m_pixelByteSize + k),
                    m_data.at(((m_height - 1 - i) * m_width + j) * m_pixelByteSize + k));
            }
        }
    }
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
