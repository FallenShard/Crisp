#include <Crisp/PathTracer/Samplers/Sampler.hpp>

namespace crisp {
Sampler::Sampler()
    : m_sampleCount(0) {}

Sampler::~Sampler() {}

size_t Sampler::getSampleCount() const {
    return m_sampleCount;
}
} // namespace crisp