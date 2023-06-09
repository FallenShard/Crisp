#include "Homogeneous.hpp"

namespace crisp
{
HomogeneousMedium::HomogeneousMedium(const VariantMap& params)
{
    m_samplingDensity = params.get<float>("density", 0.01f);

    float scale = params.get<float>("scale", 1.0f);
    m_sigmaA = params.get<Spectrum>("sigmaA", Spectrum(0.1f)) * scale;
    m_sigmaS = params.get<Spectrum>("sigmaS", Spectrum(0.1f)) * scale;
    m_sigmaT = m_sigmaA + m_sigmaS;

    auto strategy = params.get<std::string>("strategy", "balance");

    m_mediumSamplingWeight = params.get<float>("samplingWeight", -1.0f);
    if (m_mediumSamplingWeight == -1.f)
    {
        for (int i = 0; i < 3; ++i)
        {
            float albedo = m_sigmaS[i] / m_sigmaT[i];
            if (albedo > m_mediumSamplingWeight && m_sigmaT[i] != 0.0f)
                m_mediumSamplingWeight = albedo;
        }

        if (m_mediumSamplingWeight > 0.0f)
            m_mediumSamplingWeight = std::max(m_mediumSamplingWeight, 0.5f);
    }

    m_strategy = SamplingStrategy::Balance;

    m_albedo = 0.0f;
    for (int i = 0; i < 3; ++i)
    {
        if (m_sigmaT[i] != 0.0f)
            m_albedo = std::max(m_albedo, m_sigmaS[i] / m_sigmaT[i]);
    }
}

HomogeneousMedium::~HomogeneousMedium() {}

void HomogeneousMedium::eval(const Ray3& ray, Sample& sample) const
{
    eval(ray.maxT - ray.minT, sample);
}

Spectrum HomogeneousMedium::evalTransmittance(const Ray3& ray, Sampler& /*sampler*/) const
{
    float negDistance = ray.minT - ray.maxT;
    Spectrum transmittance;
    for (int i = 0; i < 3; i++)
    {
        if (m_sigmaT[i] != 0.0f)
            transmittance[i] = exp(m_sigmaT[i] * negDistance);
        else
            transmittance[i] = 1.0f;
    }

    return transmittance;
}

bool HomogeneousMedium::sampleDistance(const Ray3& ray, Medium::Sample& medSample, Sampler& sampler) const
{
    float sample = sampler.next1D();
    float sampledDistance;
    float samplingDensity = m_samplingDensity;

    if (sample < m_mediumSamplingWeight)
    {
        sample /= m_mediumSamplingWeight;

        int channel = std::min(static_cast<int>(sampler.next1D() * 3), 2);
        samplingDensity = m_sigmaT[channel];

        sampledDistance = -std::log(1.0f - sample) / samplingDensity;
    }
    else
    {
        sampledDistance = std::numeric_limits<float>::infinity();
    }

    float distToSurface = ray.maxT - ray.minT;
    bool isSuccess = true;

    if (sampledDistance < distToSurface)
    {
        medSample.t = sampledDistance + ray.minT;
        medSample.ref = ray(medSample.t);

        if (medSample.ref == ray.o)
        {
            isSuccess = false;
        }

        sampledDistance = distToSurface;
        isSuccess = false;
    }

    eval(sampledDistance, medSample);

    return isSuccess;
}

bool HomogeneousMedium::isHomogeneous() const
{
    return true;
}

void HomogeneousMedium::eval(float distance, Sample& sample) const
{
    sample.pdfSuccess = 0.0f;
    sample.pdfFailure = 0.0f;
    for (int i = 0; i < 3; ++i)
    {
        float tmp = std::exp(-m_sigmaT[i] * distance);
        sample.transmittance[i] = tmp;
        sample.pdfSuccess += m_sigmaT[i] * tmp;
        sample.pdfFailure += tmp;
    }
    sample.pdfSuccess /= 3.0f;
    sample.pdfFailure /= 3.0f;

    sample.pdfSuccess = sample.pdfSuccess * m_mediumSamplingWeight;
    sample.pdfFailure = sample.pdfFailure * m_mediumSamplingWeight + (1.0f - m_mediumSamplingWeight);
    sample.sigmaA = m_sigmaA;
    sample.sigmaS = m_sigmaS;
    sample.medium = this;

    if (sample.transmittance.maxCoeff() < 1e-10)
        sample.transmittance = Spectrum(0.0f);
}
} // namespace crisp