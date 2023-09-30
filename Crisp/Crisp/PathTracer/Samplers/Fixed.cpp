#include "Fixed.hpp"

namespace crisp {
FixedSampler::FixedSampler(const VariantMap& /*attribs*/)
    : m_index(static_cast<size_t>(0))
    , m_values{} {
    for (auto& el : m_values) {
        el = 0.5f;
    }
}

FixedSampler::~FixedSampler() {}

std::unique_ptr<Sampler> FixedSampler::clone() const {
    std::unique_ptr<FixedSampler> other = std::make_unique<FixedSampler>();
    other->m_values = m_values;
    other->m_index = m_index;

    return std::move(other);
}

void FixedSampler::prepare() {
    m_index = static_cast<size_t>(0);
}

void FixedSampler::generate() {}

void FixedSampler::advance() {}

float FixedSampler::next1D() {
    return nextFloat();
}

glm::vec2 FixedSampler::next2D() {
    return glm::vec2(nextFloat(), nextFloat());
}

float FixedSampler::nextFloat() {
    float value = m_values[m_index];
    m_index = (m_index + 1) % m_values.size();
    return value;
}
} // namespace crisp