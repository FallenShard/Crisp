#include "Image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>


namespace crisp
{
    Image::Image(std::string&& fileName, bool flipY)
        : m_width(0)
        , m_height(0)
        , m_numComponents(0)
        , m_data(nullptr)
    {
        int width, height, numComps;
        stbi_set_flip_vertically_on_load(flipY);
        m_data = stbi_load(fileName.c_str(), &width, &height, &numComps, STBI_rgb_alpha);
        stbi_set_flip_vertically_on_load(false);

        m_width         = static_cast<unsigned int>(width);
        m_height        = static_cast<unsigned int>(height);
        m_numComponents = static_cast<unsigned int>(numComps);
    }

    Image::Image(const std::string& fileName, bool flipY)
        : m_width(0)
        , m_height(0)
        , m_numComponents(0)
        , m_data(nullptr)
    {
        int width, height, numComps;
        stbi_set_flip_vertically_on_load(flipY);
        m_data = stbi_load(fileName.c_str(), &width, &height, &numComps, STBI_rgb_alpha);
        stbi_set_flip_vertically_on_load(false);

        m_width = static_cast<unsigned int>(width);
        m_height = static_cast<unsigned int>(height);
        m_numComponents = static_cast<unsigned int>(numComps);
    }

    Image::~Image()
    {
        stbi_image_free(m_data);
    }

    unsigned char* Image::getData() const
    {
        return m_data;
    }

    unsigned int Image::getWidth() const
    {
        return m_width;
    }

    unsigned int Image::getHeight() const
    {
        return m_height;
    }

    unsigned int Image::getNumComponents() const
    {
        return m_numComponents;
    }
}