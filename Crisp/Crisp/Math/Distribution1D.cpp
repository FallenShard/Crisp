#include <Crisp/Math/Distribution1D.hpp>

#include <algorithm>

namespace crisp {
namespace {

size_t findCdfInterval(const std::vector<float>& cdf, const float sampleValue) {
    const auto upper = std::ranges::upper_bound(cdf, sampleValue);
    return static_cast<size_t>(upper - cdf.begin() - 1);
}

} // namespace

Distribution1D::Distribution1D(const size_t numEntries) {
    reserve(numEntries);
    clear();
}

void Distribution1D::reserve(const size_t numEntries) {
    m_cdf.reserve(numEntries + 1);
}

void Distribution1D::clear() {
    m_cdf.assign(1, 0.0f);
    m_sum = 0.0f;
    m_normFactor = 0.0f;
    m_isNormalized = false;
}

void Distribution1D::append(const float pdfValue) {
    m_cdf.push_back(m_cdf.back() + pdfValue);
}

size_t Distribution1D::getSize() const {
    return m_cdf.size() - 1;
}

float Distribution1D::operator[](const size_t index) const {
    return m_cdf[index + 1] - m_cdf[index];
}

bool Distribution1D::isNormalized() const {
    return m_isNormalized;
}

float Distribution1D::getSum() const {
    return m_sum;
}

float Distribution1D::getNormFactor() const {
    return m_normFactor;
}

float Distribution1D::normalize() {
    m_sum = m_cdf.back();
    if (m_sum > 0.0f) {
        m_normFactor = 1.0f / m_sum;
        for (size_t i = 1; i < m_cdf.size(); ++i) {
            m_cdf[i] *= m_normFactor;
        }
    } else {
        // An all-zero input has no preferred entry. A uniform fallback keeps sampling and residual reuse defined;
        // getSum() and getNormFactor() remain zero so callers can still detect that the original measure vanished.
        const float inverseSize = 1.0f / static_cast<float>(getSize());
        for (size_t i = 1; i < m_cdf.size(); ++i) {
            m_cdf[i] = static_cast<float>(i) * inverseSize;
        }
        m_normFactor = 0.0f;
    }

    m_cdf.back() = 1.0f;
    m_isNormalized = true;
    return m_sum;
}

float Distribution1D::sampleContinuous(const float sampleValue) const {
    float pdf = 0.0f;
    return sampleContinuous(sampleValue, pdf);
}

float Distribution1D::sampleContinuous(const float sampleValue, float& pdf) const {
    size_t offset = 0;
    return sampleContinuous(sampleValue, pdf, offset);
}

float Distribution1D::sampleContinuous(const float sampleValue, float& pdf, size_t& offset) const {
    offset = findCdfInterval(m_cdf, sampleValue);

    const float probability = operator[](offset);
    const float residual = (sampleValue - m_cdf[offset]) / probability;
    pdf = probability * static_cast<float>(getSize());
    return (static_cast<float>(offset) + residual) / static_cast<float>(getSize());
}

size_t Distribution1D::sample(const float sampleValue) const {
    return findCdfInterval(m_cdf, sampleValue);
}

size_t Distribution1D::sample(const float sampleValue, float& probability) const {
    const size_t index = sample(sampleValue);
    probability = operator[](index);
    return index;
}

size_t Distribution1D::sampleReuse(float& sampleValue) const {
    float probability = 0.0f;
    return sampleReuse(sampleValue, probability);
}

size_t Distribution1D::sampleReuse(float& sampleValue, float& probability) const {
    const size_t index = sample(sampleValue, probability);
    sampleValue = (sampleValue - m_cdf[index]) / probability;
    return index;
}

} // namespace crisp
