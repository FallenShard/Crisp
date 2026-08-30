#pragma once

#include <cstddef>
#include <vector>

namespace crisp {
class Distribution1D {
public:
    explicit Distribution1D(size_t numEntries = 0);

    void reserve(size_t numEntries);
    void clear();
    // Precondition: called before normalize(); pdfValue and the resulting cumulative sum are finite and non-negative.
    void append(float pdfValue);

    size_t getSize() const;
    float operator[](size_t index) const;

    bool isNormalized() const;
    float getSum() const;
    float getNormFactor() const;

    // Precondition: the distribution is non-empty and has not been normalized since construction or clear().
    float normalize();

    // Precondition: normalized and non-empty; sampleValue is finite and in [0, 1).
    float sampleContinuous(float sampleValue) const;
    // Samples a piecewise-constant density over [0, 1). The returned PDF is a continuous density, not a cell
    // probability.
    float sampleContinuous(float sampleValue, float& pdf) const;
    float sampleContinuous(float sampleValue, float& pdf, size_t& offset) const;

    // Discrete sampling APIs return the selected cell's probability mass.
    // Precondition: normalized and non-empty; sampleValue is finite and in [0, 1).
    size_t sample(float sampleValue) const;
    size_t sample(float sampleValue, float& probability) const;
    // Precondition: normalized and non-empty; sampleValue is finite and in [0, 1).
    size_t sampleReuse(float& sampleValue) const;
    size_t sampleReuse(float& sampleValue, float& probability) const;

private:
    std::vector<float> m_cdf;
    float m_sum{0.0f};
    float m_normFactor{0.0f};
    bool m_isNormalized{false};
};
} // namespace crisp
