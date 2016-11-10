#pragma once

#include <string>

namespace crisp
{
    class Image
    {
    public:
        Image(std::string&& fileName, bool flipY = false);
        ~Image();

        unsigned char* getData() const;
        int getWidth() const;
        int getHeight() const;
        int getNumComponents() const;

    private:
        unsigned char* m_data;
        int m_width;
        int m_height;
        int m_numComponents;
    };
}