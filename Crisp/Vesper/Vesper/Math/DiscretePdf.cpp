#include "DiscretePdf.hpp"

namespace vesper
{
    DiscretePdf::DiscretePdf(size_t numEntries)
    {
        reserve(numEntries);
        clear();
    }

    DiscretePdf::~DiscretePdf()
    {
    }

    void DiscretePdf::reserve(size_t numEntries)
    {
        m_cdf.reserve(numEntries + 1);
    }

    void DiscretePdf::clear()
    {
        m_cdf.clear();
        m_cdf.push_back(0.f);
        m_isNormalized = false;
    }

    void DiscretePdf::append(float pdfValue)
    {
        m_cdf.push_back(m_cdf[m_cdf.size() - 1] + pdfValue);
    }

    size_t DiscretePdf::getSize() const
    {
        return m_cdf.size() - 1;
    }

    float DiscretePdf::operator[](size_t index) const
    {
        return m_cdf[index + 1] - m_cdf[index];
    }

    bool DiscretePdf::isNormalized() const
    {
        return m_isNormalized;
    }

    float DiscretePdf::getSum() const
    {
        return m_sum;
    }

    float DiscretePdf::getNormFactor() const
    {
        return m_normFactor;
    }

    float DiscretePdf::normalize()
    {
        m_sum = m_cdf[m_cdf.size() - 1];
        if (m_sum > 0.f)
        {
            m_normFactor = 1.f / m_sum;
            for (size_t i = 1; i < m_cdf.size(); ++i)
                m_cdf[i] *= m_normFactor;

            m_cdf[m_cdf.size() - 1] = 1.f;
            m_isNormalized = true;
        }
        else
        {
            m_normFactor = 0.f;
        }

        return m_sum;
    }

    size_t DiscretePdf::sample(float sampleValue) const
    {
        auto entry = std::lower_bound(m_cdf.begin(), m_cdf.end(), sampleValue);
        size_t index = static_cast<size_t>(std::max(static_cast<ptrdiff_t>(0), entry - m_cdf.begin() - 1));
        return std::min(index, m_cdf.size() - 2);
    }

    size_t DiscretePdf::sample(float sampleValue, float& pdf) const
    {
        size_t index = sample(sampleValue);
        pdf = operator[](index);
        return index;
    }

    size_t DiscretePdf::sampleReuse(float& sampleValue) const
    {
        size_t index = sample(sampleValue);
        sampleValue = (sampleValue - m_cdf[index]) / (m_cdf[index + 1] - m_cdf[index]);
        return index;
    }

    size_t DiscretePdf::sampleReuse(float& sampleValue, float& pdf) const
    {
        size_t index = sample(sampleValue, pdf);
        sampleValue = (sampleValue - m_cdf[index]) / (m_cdf[index + 1] - m_cdf[index]);
        return index;
    }
}