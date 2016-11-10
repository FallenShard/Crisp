#include "Image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>


namespace crisp
{
    Image::Image(std::string&& fileName, bool flipY)
        : m_width(-1)
        , m_height(-1)
        , m_numComponents(-1)
        , m_data(nullptr)
    {
        stbi_set_flip_vertically_on_load(flipY);
        m_data = stbi_load(fileName.c_str(), &m_width, &m_height, &m_numComponents, STBI_rgb_alpha);
        stbi_set_flip_vertically_on_load(false);
    }

    Image::~Image()
    {
        stbi_image_free(m_data);
    }

    unsigned char* Image::getData() const
    {
        return m_data;
    }

    int Image::getWidth() const
    {
        return m_width;
    }

    int Image::getHeight() const
    {
        return m_height;
    }

    int Image::getNumComponents() const
    {
        return m_numComponents;
    }
}