#pragma once

#include <memory>

#include "Core/VariantMap.hpp"
#include "ReconstructionFilters/ReconstructionFilter.hpp"
#include <Crisp/Spectra/Spectrum.hpp>
#include <Crisp/Math/Ray.hpp>

namespace crisp
{
class ReconstructionFilter;

class Camera
{
public:
    virtual ~Camera() = default;
    virtual Spectrum sampleRay(Ray3& ray, const glm::vec2& samplePosition, const glm::vec2& apertureSample) const = 0;

    glm::ivec2 getImageSize() const;
    const ReconstructionFilter* getReconstructionFilter() const;

protected:
    glm::ivec2 m_imageSize;

    std::unique_ptr<ReconstructionFilter> m_filter;
};
} // namespace crisp