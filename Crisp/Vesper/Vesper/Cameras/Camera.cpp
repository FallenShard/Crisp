#include "Camera.hpp"



namespace vesper
{
    glm::ivec2 Camera::getImageSize() const
    {
        return m_imageSize;
    }

    const ReconstructionFilter* Camera::getReconstructionFilter() const
    {
        return m_filter.get();
    }
}