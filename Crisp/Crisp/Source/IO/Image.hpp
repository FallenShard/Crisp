#pragma once

#include <string>

namespace crisp
{
    class Image
    {
    public:
        Image(std::string&& fileName, bool flipY = false);
        Image(const std::string& fileName, bool flipY = false);
        ~Image();

        unsigned char* getData() const;
        unsigned int getWidth() const;
        unsigned int getHeight() const;
        unsigned int getNumComponents() const;

    private:
        unsigned char* m_data;
        unsigned int m_width;
        unsigned int m_height;
        unsigned int m_numComponents;
    };
}