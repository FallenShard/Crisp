#include "ImageFileBuffer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <algorithm>

namespace crisp
{
    auto getStbComponentFormat(int numComponents)
    {
        switch (numComponents)
        {
            case 0:  return STBI_default;
            case 1:  return STBI_grey;
            case 2:  return STBI_grey_alpha;
            case 3:  return STBI_rgb;
            case 4:  return STBI_rgb_alpha;
            default: return STBI_default;
        }
    }

    ImageFileBuffer::ImageFileBuffer(unsigned int width, unsigned int height, unsigned int numComponents, std::vector<unsigned char> data)
        : m_width(width)
        , m_height(height)
        , m_numComponents(numComponents)
        , m_data(data)
    {
    }

    ImageFileBuffer::ImageFileBuffer(const std::filesystem::path& filePath, int requestedComponents, bool flipY)
        : m_width(0)
        , m_height(0)
        , m_numComponents(0)
    {
        int width, height, numComps;
        stbi_set_flip_vertically_on_load(flipY);
        stbi_uc* data = stbi_load(filePath.string().c_str(), &width, &height, &numComps, getStbComponentFormat(requestedComponents));
        stbi_set_flip_vertically_on_load(false);

        if (!data)
            throw std::runtime_error("Failed to load image " + filePath.string());

        m_width         = static_cast<unsigned int>(width);
        m_height        = static_cast<unsigned int>(height);
        m_numComponents = static_cast<unsigned int>(requestedComponents);
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

    uint32_t ImageFileBuffer::getMipLevels() const
    {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;
    }

    HdrImageFileBuffer::HdrImageFileBuffer(const std::filesystem::path& filePath, int requestedComponents, bool flipY)
        : m_width(0)
        , m_height(0)
        , m_numComponents(0)
    {
        int width, height, numComps;
        stbi_set_flip_vertically_on_load(flipY);
        float* data = stbi_loadf(filePath.string().c_str(), &width, &height, &numComps, getStbComponentFormat(requestedComponents));
        stbi_set_flip_vertically_on_load(false);

        if (!data)
            throw std::runtime_error("Failed to load image " + filePath.string());

        m_width = static_cast<unsigned int>(width);
        m_height = static_cast<unsigned int>(height);
        m_numComponents = static_cast<unsigned int>(requestedComponents);
        m_data.resize(m_width * m_height * m_numComponents);
        memcpy(m_data.data(), data, m_width * m_height * m_numComponents * sizeof(float));
    }

    const float* HdrImageFileBuffer::getData() const
    {
        return m_data.data();
    }

    unsigned int HdrImageFileBuffer::getWidth() const
    {
        return m_width;
    }

    unsigned int HdrImageFileBuffer::getHeight() const
    {
        return m_height;
    }

    unsigned int HdrImageFileBuffer::getNumComponents() const
    {
        return m_numComponents;
    }

    uint64_t HdrImageFileBuffer::getByteSize() const
    {
        return m_width * m_height * m_numComponents * sizeof(float);
    }

    uint32_t HdrImageFileBuffer::getMipLevels() const
    {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;
    }
}