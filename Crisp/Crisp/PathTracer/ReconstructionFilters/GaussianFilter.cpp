#include <Crisp/PathTracer/ReconstructionFilters/GaussianFilter.hpp>

#include <cmath>

namespace crisp {
GaussianFilter::GaussianFilter() {
    m_sigma = 0.5f;
    m_radius = 2.0f;

    initLookupTable();
}

float GaussianFilter::eval(float x, float y) const {
    float alpha = -1.0f / (2.0f * m_sigma * m_sigma);
    float evalX = std::max(0.0f, std::exp(alpha * x * x) - std::exp(alpha * m_radius * m_radius));
    float evalY = std::max(0.0f, std::exp(alpha * y * y) - std::exp(alpha * m_radius * m_radius));
    return evalX * evalY;
}
} // namespace crisp