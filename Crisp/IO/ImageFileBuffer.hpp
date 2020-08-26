#pragma once

#include <filesystem>
#include <vector>

namespace crisp
{
    class ImageFileBuffer
    {
    public:
        ImageFileBuffer(const std::filesystem::path& filePath, int requestedComponents = 4, bool flipY = false);

        const unsigned char* getData() const;

        template <typename T>
        inline const T* getData() const
        {
            return static_cast<const T*>(m_data.data());
        }

        unsigned int getWidth() const;
        unsigned int getHeight() const;
        unsigned int getNumComponents() const;

        uint64_t getPixelByteSize() const;
        uint64_t getByteSize() const;

        uint32_t getMipLevels() const;

        static uint32_t getMipLevels(uint32_t width, uint32_t height);

    private:
        std::vector<unsigned char> m_data;
        unsigned int m_width;
        unsigned int m_height;
        unsigned int m_numComponents;
        std::size_t  m_pixelByteSize;
    };
}
