#include "ImageFileBuffer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace crisp
{
    ImageFileBuffer::ImageFileBuffer(const std::string& fileName, bool flipY)
        : m_width(0)
        , m_height(0)
        , m_numComponents(0)
    {
        int width, height, numComps;
        stbi_set_flip_vertically_on_load(flipY);
        stbi_uc* data = stbi_load(fileName.c_str(), &width, &height, &numComps, STBI_rgb_alpha);
        stbi_set_flip_vertically_on_load(false);

        if (!data)
            throw std::runtime_error("Failed to load image " + fileName);

        m_width         = static_cast<unsigned int>(width);
        m_height        = static_cast<unsigned int>(height);
        m_numComponents = static_cast<unsigned int>(numComps);
        m_data.resize(m_width * m_height * m_numComponents);
        memcpy(m_data.data(), data, m_width * m_height * m_numComponents);
    }

    const unsigned char* ImageFileBuffer::getData() const
    {
        return m_data.data();
    }

    unsigned int ImageFileBuffer::getWidth() const
    {
        return m_width;
    }

    unsigned int ImageFileBuffer::getHeight() const
    {
        return m_height;
    }

    unsigned int ImageFileBuffer::getNumComponents() const
    {
        return m_numComponents;
    }

    uint64_t ImageFileBuffer::getByteSize() const
    {
        return m_width * m_height * m_numComponents;
    }

    void ImageFileBuffer::padComponents(int numComponents)
    {
        std::vector<unsigned char> default(numComponents, 0);
        default.back() = 255;

        if (m_numComponents == numComponents)
            return;

        std::vector<unsigned char> newData(m_numComponents * m_width * m_height);
        for (int i = 0; i < m_height; i++)
        {
            for (int j = 0; j < m_width; j++)
            {
                int texelIdx = i * m_width + j;
                for (int k = 0; k < m_numComponents; k++)
                {
                    newData[numComponents * texelIdx + k] = m_data[m_numComponents * texelIdx + k];
                }

                for (int k = m_numComponents; k < numComponents; k++)
                {
                    newData[numComponents * texelIdx + k] = default[k];
                }
            }
        }

        m_numComponents = numComponents;
        std::swap(m_data, newData);
    }
}