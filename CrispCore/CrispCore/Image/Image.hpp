#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace crisp
{
    class Image
    {
    public:
        Image(std::vector<uint8_t> pixelData, uint32_t width, uint32_t height, uint32_t channelCount, uint32_t pixelByteSize);

        template <typename T = uint8_t>
        inline const T* getData() const
        {
            return static_cast<const T*>(m_data.data());
        }

        uint32_t getWidth() const;
        uint32_t getHeight() const;
        uint32_t getChannelCount() const;

        uint32_t getPixelByteSize() const;
        uint64_t getByteSize() const;

        uint32_t getMipLevels() const;

        static uint32_t getMipLevels(uint32_t width, uint32_t height);

        static Image createDefaultAlbedoMap(const std::array<uint8_t, 4>& color = { 255, 0, 255, 255 });
        static Image createDefaultNormalMap();
        static Image createDefaultMetallicMap(float metallic = 0.0f);
        static Image createDefaultRoughnessMap(float roughness = 0.125f);
        static Image createDefaultAmbientOcclusionMap();

    private:
        std::vector<uint8_t> m_data;
        uint32_t m_width;
        uint32_t m_height;
        uint32_t m_channelCount;
        uint32_t m_pixelByteSize;
    };
}
