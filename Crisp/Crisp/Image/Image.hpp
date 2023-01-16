#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace crisp
{
class Image
{
public:
    Image();
    Image(
        std::vector<uint8_t> pixelData, uint32_t width, uint32_t height, uint32_t channelCount, uint32_t pixelByteSize);

    template <typename T = uint8_t>
    inline const T* getData() const
    {
        return static_cast<const T*>(m_data.data());
    }

    Image createFromChannel(uint32_t channelIndex) const;
    Image createSubImage(uint32_t row, uint32_t col, uint32_t width, uint32_t height) const;

    void transpose();
    void mirrorX();
    void mirrorY();

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    uint32_t getChannelCount() const;

    uint32_t getPixelByteSize() const;
    uint64_t getByteSize() const;

    uint32_t getMipLevels() const;

    static uint32_t getMipLevels(uint32_t width, uint32_t height);

private:
    std::vector<uint8_t> m_data;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_channelCount;
    uint32_t m_pixelByteSize;
};
} // namespace crisp
