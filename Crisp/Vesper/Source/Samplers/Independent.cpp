#include "Independent.hpp"

namespace vesper
{
    IndependentSampler::IndependentSampler(const VariantMap& attribs)
        : m_randomEngine(std::random_device()())
        , m_distribution(0.0f, 1.0f)
    {
        m_sampleCount = attribs.get("numSamples", 64);
    }

    IndependentSampler::~IndependentSampler()
    {
    }

    std::unique_ptr<Sampler> IndependentSampler::clone() const
    {
        std::unique_ptr<IndependentSampler> other = std::make_unique<IndependentSampler>();
        other->m_randomEngine = m_randomEngine;
        other->m_distribution = m_distribution;
        other->m_sampleCount = m_sampleCount;

        return std::move(other);
    }

    void IndependentSampler::prepare()
    {
        m_randomEngine.seed(std::random_device()());
    }

    void IndependentSampler::generate()
    {
    }

    void IndependentSampler::advance()
    {
    }

    float IndependentSampler::next1D()
    {
        return nextFloat();
    }

    glm::vec2 IndependentSampler::next2D()
    {
        return glm::vec2(nextFloat(), nextFloat());
    }

    float IndependentSampler::nextFloat()
    {
        return m_distribution(m_randomEngine);
    }
}