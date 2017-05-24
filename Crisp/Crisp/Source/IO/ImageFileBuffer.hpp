#pragma once

#include <string>

namespace crisp
{
    class ImageFileBuffer
    {
    public:
        ImageFileBuffer(const std::string& fileName, bool flipY = false);
        ~ImageFileBuffer();

        unsigned char* getData() const;
        unsigned int getWidth() const;
        unsigned int getHeight() const;
        unsigned int getNumComponents() const;
        uint64_t getByteSize() const;

    private:
        unsigned char* m_data;
        unsigned int m_width;
        unsigned int m_height;
        unsigned int m_numComponents;
    };
}