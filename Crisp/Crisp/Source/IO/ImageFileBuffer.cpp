#include "ImageFileBuffer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 4244) // conversion warnings
#include <stb/stb_image.h>
#pragma warning(pop)

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

    ImageFileBuffer::ImageFileBuffer(const std::filesystem::path& filePath, int requestedComponents, bool flipY)
        : m_width(0)
        , m_height(0)
        , m_numComponents(0)
    {
        int width, height, numComps;
        stbi_set_flip_vertically_on_load(flipY);

        std::size_t typeSize = 1;
        void* dataPtr = nullptr;
        std::string ext = filePath.extension().string();
        if (ext == ".hdr")
        {
            typeSize = sizeof(float);
            dataPtr = stbi_loadf(filePath.string().c_str(), &width, &height, &numComps, getStbComponentFormat(requestedComponents));
        }
        else
        {
            typeSize = sizeof(unsigned char);
            dataPtr = stbi_load(filePath.string().c_str(), &width, &height, &numComps, getStbComponentFormat(requestedComponents));
        }

        stbi_set_flip_vertically_on_load(false);

        if (!dataPtr)
            throw std::runtime_error("Failed to load image " + filePath.string());

        m_width         = static_cast<unsigned int>(width);
        m_height        = static_cast<unsigned int>(height);
        m_numComponents = static_cast<unsigned int>(requestedComponents);
        m_pixelByteSize = m_numComponents * typeSize;

        m_data.resize(m_width * m_height * m_pixelByteSize);
        memcpy(m_data.data(), dataPtr, m_width * m_height * m_pixelByteSize);

        stbi_image_free(dataPtr);
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

    uint64_t ImageFileBuffer::getPixelByteSize() const
    {
        return m_pixelByteSize;
    }

    uint64_t ImageFileBuffer::getByteSize() const
    {
        return m_data.size();
    }

    uint32_t ImageFileBuffer::getMipLevels() const
    {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;
    }

    uint32_t ImageFileBuffer::getMipLevels(uint32_t width, uint32_t height)
    {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    }
}