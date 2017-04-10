#pragma once

#include <algorithm>
#include <vector>

namespace vesper
{
    class Distribution1D
    {
    public:
        Distribution1D(size_t numEntries = 0);
        ~Distribution1D();

        void reserve(size_t numEntries);
        void clear();
        void append(float pdfValue);

        size_t getSize() const;
        float operator[](size_t index) const;

        bool isNormalized() const;
        float getSum() const;
        float getNormFactor() const;

        float normalize();

        float sampleContinuous(float sampleValue) const;
        float sampleContinuous(float sampleValue, float& pdf) const;
        float sampleContinuous(float sampleValue, float& pdf, size_t& offset) const;

        size_t sample(float sampleValue) const;
        size_t sample(float sampleValue, float& pdf) const;
        size_t sampleReuse(float& sampleValue) const;
        size_t sampleReuse(float& sampleValue, float& pdf) const;

    private:
        std::vector<float> m_cdf;
        float m_sum;
        float m_normFactor;
        bool m_isNormalized;
    };
}