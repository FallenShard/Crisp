#pragma once

#include <string>
#include <vector>

namespace crisp
{
    class ImageFileBuffer
    {
    public:
        ImageFileBuffer(const std::string& fileName, int requestedComponents = 4, bool flipY = false);

        const unsigned char* getData() const;
        unsigned int getWidth() const;
        unsigned int getHeight() const;
        unsigned int getNumComponents() const;
        uint64_t getByteSize() const;

        uint32_t getMipLevels() const;

    private:
        std::vector<unsigned char> m_data;
        unsigned int m_width;
        unsigned int m_height;
        unsigned int m_numComponents;
    };
}