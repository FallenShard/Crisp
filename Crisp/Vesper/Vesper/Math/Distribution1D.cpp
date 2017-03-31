#include "Distribution1D.hpp"

namespace vesper
{
    Distribution1D::Distribution1D(size_t numEntries)
    {
        reserve(numEntries);
        clear();
    }

    Distribution1D::~Distribution1D()
    {
    }

    void Distribution1D::reserve(size_t numEntries)
    {
        m_cdf.reserve(numEntries + 1);
    }

    void Distribution1D::clear()
    {
        m_cdf.clear();
        m_cdf.push_back(0.0f);
        m_isNormalized = false;
    }

    void Distribution1D::append(float pdfValue)
    {
        m_cdf.push_back(m_cdf.back() + pdfValue);
    }

    size_t Distribution1D::getSize() const
    {
        return m_cdf.size() - 1;
    }

    float Distribution1D::operator[](size_t index) const
    {
        return m_cdf[index + 1] - m_cdf[index];
    }

    bool Distribution1D::isNormalized() const
    {
        return m_isNormalized;
    }

    float Distribution1D::getSum() const
    {
        return m_sum;
    }

    float Distribution1D::getNormFactor() const
    {
        return m_normFactor;
    }

    float Distribution1D::normalize()
    {
        m_sum = m_cdf.back();
        if (m_sum > 0.0f)
        {
            m_normFactor = 1.0f / m_sum;
            for (size_t i = 1; i < m_cdf.size(); ++i)
                m_cdf[i] *= m_normFactor;

            m_cdf[m_cdf.size() - 1] = 1.0f;
            m_isNormalized = true;
        }
        else
        {
            m_normFactor = 0.0f;
        }

        return m_sum;
    }

    float Distribution1D::sampleContinuous(float sampleValue) const
    {
        auto entry = std::lower_bound(m_cdf.begin(), m_cdf.end(), sampleValue);
        size_t index = static_cast<size_t>(std::max(static_cast<ptrdiff_t>(0), entry - m_cdf.begin() - 1));
        index = std::min(index, m_cdf.size() - 2);

        float du = sampleValue - m_cdf[index];
        auto pdf = operator[](index);
        if (pdf > 0.0f)
            du /= pdf;

        return (index + du) / static_cast<float>(getSize());
    }

    float Distribution1D::sampleContinuous(float sampleValue, float& pdf) const
    {
        auto entry = std::lower_bound(m_cdf.begin(), m_cdf.end(), sampleValue);
        size_t index = static_cast<size_t>(std::max(static_cast<ptrdiff_t>(0), entry - m_cdf.begin() - 1));
        index = std::min(index, m_cdf.size() - 2);

        pdf = operator[](index);

        float du = sampleValue - m_cdf[index];
        if (pdf > 0.0f)
            du /= pdf;

        return (index + du) / static_cast<float>(getSize());
    }

    size_t Distribution1D::sample(float sampleValue) const
    {
        auto entry = std::lower_bound(m_cdf.begin(), m_cdf.end(), sampleValue);
        size_t index = static_cast<size_t>(std::max(static_cast<ptrdiff_t>(0), entry - m_cdf.begin() - 1));
        return std::min(index, m_cdf.size() - 2);
    }

    size_t Distribution1D::sample(float sampleValue, float& pdf) const
    {
        size_t index = sample(sampleValue);
        pdf = operator[](index);
        return index;
    }

    size_t Distribution1D::sampleReuse(float& sampleValue) const
    {
        size_t index = sample(sampleValue);
        sampleValue = (sampleValue - m_cdf[index]) / (m_cdf[index + 1] - m_cdf[index]);
        return index;
    }

    size_t Distribution1D::sampleReuse(float& sampleValue, float& pdf) const
    {
        size_t index = sample(sampleValue, pdf);
        sampleValue = (sampleValue - m_cdf[index]) / (m_cdf[index + 1] - m_cdf[index]);
        return index;
    }
}