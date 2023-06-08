#include "EnvironmentLight.hpp"

#include <Crisp/Math/Distribution1D.hpp>
#include <Crisp/Math/Operations.hpp>
#include <Crisp/Math/Warp.hpp>

#include <Crisp/PathTracer/Samplers/Sampler.hpp>
#include <Crisp/PathTracer/Shapes/Shape.hpp>

#include <Crisp/PathTracer/Core/MipMap.hpp>

#include <Crisp/PathTracer/Samplers/SamplerFactory.hpp>

namespace crisp
{
namespace
{
inline uint32_t sampleReuse(float* cdf, uint32_t size, float& sample)
{
    float* entry = std::lower_bound(cdf, cdf + size + 1, sample);
    uint32_t index = std::min((uint32_t)std::max((ptrdiff_t)0, entry - cdf - 1), size - 1);
    sample = (sample - cdf[index]) / (cdf[index + 1] - cdf[index]);
    return index;
}

float intervalToTent(float sample)
{
    float sign;

    if (sample < 0.5f)
    {
        sign = 1;
        sample *= 2;
    }
    else
    {
        sign = -1;
        sample = 2 * (sample - 0.5f);
    }

    return sign * (1 - std::sqrt(sample));
}

inline glm::vec2 squareToTent(const glm::vec2& sample)
{
    return glm::vec2(intervalToTent(sample.x), intervalToTent(sample.y));
}
} // namespace

EnvironmentLight::EnvironmentLight(const VariantMap& params)
{
    auto probeFilename = params.get<std::string>("fileName", "uffizi-large.exr");
    m_scale = params.get<float>("scale", 1.0f);

    m_probe = std::make_unique<MipMap>("../../Resources/Textures/" + probeFilename);

    int w = m_probe->getWidth();
    int h = m_probe->getHeight();

    size_t numEntries = (w + 1) * h;

    m_cdfCols.resize(numEntries);
    m_cdfRows.resize(h + 1);
    m_rowWeights.resize(h);

    size_t colPos = 0;
    size_t rowPos = 0;
    float rowSum = 0.0f;

    m_cdfRows[rowPos++] = 0;
    for (int y = 0; y < h; y++)
    {
        float colSum = 0.0f;

        m_cdfCols[colPos++] = 0;
        for (int x = 0; x < w; x++)
        {
            Spectrum val = m_probe->fetch(x, y);

            colSum += val.getLuminance();
            m_cdfCols[colPos++] = colSum;
        }

        float normalization = 1.0f / colSum;
        for (int x = 1; x < w; x++)
            m_cdfCols[colPos - x - 1] *= normalization;
        m_cdfCols[colPos - 1] = 1.0f;

        float weight = std::sin((y + 0.5f) * PI<> / h);
        m_rowWeights[y] = weight;
        rowSum += colSum * weight;
        m_cdfRows[rowPos++] = rowSum;
    }

    float normalization = 1.0f / rowSum;
    for (int y = 1; y < h; y++)
        m_cdfRows[rowPos - y - 1] *= normalization;
    m_cdfRows[rowPos - 1] = 1.0f;

    m_normalization = 1.0f / (rowSum * (2.0f * PI<> / w) * (PI<> / h));

    //// h pdfs for individual rows
    // m_phiPdfs.resize(h);
    //
    //// pdf for row pdfs
    // m_thetaPdf.reserve(h);
    //
    // m_rowWeights.resize(h);
    //
    // for (int i = 0; i < h; i++)
    //{
    //     auto& phiPdf = m_phiPdfs[i];
    //     phiPdf.reserve(w);
    //
    //     float theta = (i + 0.5f) * PI / static_cast<float>(h);
    //     float sinTheta = std::sinf(theta);
    //
    //     float v = static_cast<float>(i) / h;
    //
    //     for (int j = 0; j < w; j++)
    //     {
    //         float u = static_cast<float>(j) / w;
    //         phiPdf.append(m_probe->evalLerp(u, v).getLuminance() * sinTheta);
    //     }
    //
    //     m_rowWeights[i] = sinTheta;
    //
    //     float sum = phiPdf.normalize();
    //     m_thetaPdf.append(sum);
    // }
    //
    // auto sum = m_thetaPdf.normalize();

    m_pixelSize = glm::vec2(2 * PI<> / w, PI<> / h);

    Light::Sample s;
    s.ref = glm::vec3(0, 0, 0);

    s.wi = glm::vec3(0, 0, -1);

    auto fixed = SamplerFactory::create("fixed", VariantMap());

    auto samVal = sample(s, *fixed);

    auto ev = eval(s);
    // auto p = pdf(s);
}

EnvironmentLight::~EnvironmentLight() {}

Spectrum EnvironmentLight::eval(const Light::Sample& sample) const
{
    float u = (1.0f + atan2(sample.wi.x, -sample.wi.z) * InvPI<>)*0.5f;
    float v = acosf(std::min(1.0f, sample.wi.y)) * InvPI<>;

    return m_scale * m_probe->evalLerp(u, v);
}

Spectrum EnvironmentLight::sample(Light::Sample& sample, Sampler& sampler) const
{
    glm::vec2 point = sampler.next2D();
    uint32_t row = sampleReuse((float*)&m_cdfRows[0], m_probe->getHeight(), point.y);
    uint32_t col = sampleReuse((float*)&m_cdfCols[0] + row * (m_probe->getWidth() + 1), m_probe->getWidth(), point.x);

    glm::vec2 scramble = squareToTent(point);
    glm::vec2 pos = glm::vec2((float)col, (float)row) + scramble;

    int xPos = (int)std::floor(pos.x);
    int yPos = (int)std::floor(pos.y);
    float dx1 = pos.x - xPos;
    float dx2 = 1.0f - dx1;
    float dy1 = pos.y - yPos;
    float dy2 = 1.0f - dy1;

    Spectrum val1 = m_probe->fetch(xPos, yPos) * dx2 * dy2 + m_probe->fetch(xPos + 1, yPos) * dx1 * dy2;
    Spectrum val2 = m_probe->fetch(xPos, yPos + 1) * dx2 * dy1 + m_probe->fetch(xPos + 1, yPos + 1) * dx1 * dy1;

    Spectrum val = (val1 + val2) * m_scale;
    float pdf = (val1.getLuminance() * m_rowWeights[clamp(yPos, 0, m_probe->getHeight())] +
                 val2.getLuminance() * m_rowWeights[clamp(yPos + 1, 0, m_probe->getHeight())]) *
                m_normalization;

    auto theta = PI<> * (pos.y + 0.5f) / m_probe->getHeight();

    float x = (pos.x + 0.5f) / m_probe->getWidth();
    auto phi = PI<> * (2.0f * x - 1.0f);

    float sinTheta = sinf(theta);
    glm::vec3 dir(sinTheta * sinf(phi), cosf(theta), -sinTheta * cosf(phi));

    pdf /= std::max(std::abs(sinTheta), Ray3::Epsilon);

    // sample.p = ;   // In(eval), Out(sample) - Point on the emitter
    // sample.n = glm::normalize(m_sceneCenter - sample.p);
    sample.wi = dir;
    sample.pdf = pdf;
    sample.shadowRay = Ray3(sample.ref, sample.wi, Ray3::Epsilon, m_sceneRadius * 2.0f);

    return val / pdf;
    // auto point = sampler.next2D();
    //
    // float scPdf;
    // auto contS = sampleContinuous(point, scPdf);
    //
    // auto theta = PI * contS.y;
    // auto phi = PI * (2.0f * contS.x - 1.0f);
    //
    // auto sinTheta = std::sinf(theta);
    //
    // sample.wi.x = sinTheta * sinf(phi);
    // sample.wi.y = cosf(theta);
    // sample.wi.z = -sinTheta * cosf(phi);
    // sample.pdf = scPdf * InvTwoPI / sinTheta;
    //
    // sample.shadowRay = Ray3(sample.ref, sample.wi, Ray3::Epsilon, m_sceneRadius * 2.0f);
    //
    // return m_scale * m_probe->evalLerp(contS.x, contS.y) / sample.pdf;
}

float EnvironmentLight::pdf(const Light::Sample& sample) const
{
    float u = (1.0f + atan2(sample.wi.x, -sample.wi.z) * InvPI<>)*0.5f;
    float v = acosf(sample.wi.y) * InvPI<>;

    float fracU = u * m_probe->getWidth() - 0.5f;
    float fracV = v * m_probe->getHeight() - 0.5f;

    int xPos = (int)std::floor(fracU);
    int yPos = (int)std::floor(fracV);
    float dx1 = fracU - xPos;
    float dx2 = 1.0f - dx1;
    float dy1 = fracV - yPos;
    float dy2 = 1.0f - dy1;

    Spectrum val1 = m_probe->fetch(xPos, yPos) * dx2 * dy2 + m_probe->fetch(xPos + 1, yPos) * dx1 * dy2;
    Spectrum val2 = m_probe->fetch(xPos, yPos + 1) * dx2 * dy1 + m_probe->fetch(xPos + 1, yPos + 1) * dx1 * dy1;

    Spectrum val = (val1 + val2) * m_scale;

    float sinTheta = sqrt(1.0f - sample.wi.y * sample.wi.y);

    float pdf = (val1.getLuminance() * m_rowWeights[clamp(yPos, 0, m_probe->getHeight())] +
                 val2.getLuminance() * m_rowWeights[clamp(yPos + 1, 0, m_probe->getHeight())]) *
                m_normalization;

    pdf /= std::max(std::abs(sinTheta), Ray3::Epsilon);

    return pdf;
    // float theta = acosf(sample.wi.y);
    // float u = (1.0f + atan2(sample.wi.x, -sample.wi.z) * InvPI) * 0.5f;
    // float v = theta * InvPI;
    //
    // float p = distributionPdf(u, v);
    //
    // return p * InvTwoPI / sin(theta);
}

Spectrum EnvironmentLight::samplePhoton(Ray3& /*ray*/, Sampler& /*sampler*/) const
{
    return Spectrum();
}

void EnvironmentLight::setBoundingSphere(const glm::vec4& sphereParams)
{
    m_sceneCenter.x = sphereParams.x;
    m_sceneCenter.y = sphereParams.y;
    m_sceneCenter.z = sphereParams.z;
    m_sceneRadius = sphereParams.w;

    m_power = 4 * PI<> * m_sceneRadius * m_sceneRadius * m_scale / m_normalization;
}

bool EnvironmentLight::isDelta() const
{
    return false;
}

glm::vec2 EnvironmentLight::sampleContinuous(const glm::vec2& /*point*/, float& /*pdf*/) const
{
    // float uPdf = 0.0f;
    // float vPdf = 0.0f;
    // size_t vOffset = 0;
    //
    // glm::vec2 result;
    // result.y = m_thetaPdf.sampleContinuous(point.x, vPdf, vOffset);
    // result.x = m_phiPdfs[vOffset].sampleContinuous(point.y, uPdf);
    //
    // pdf = uPdf * vPdf;
    // return result;
    return {0.0f, 0.0f};
}

float EnvironmentLight::distributionPdf(float /*u*/, float /*v*/) const
{
    // int uOffset = clamp(int(u * m_phiPdfs[0].getSize()), 0, int(m_phiPdfs[0].getSize() - 1));
    // int vOffset = clamp(int(v * m_thetaPdf.getSize()), 0, int(m_thetaPdf.getSize() - 1));
    //
    // return m_phiPdfs[vOffset][uOffset];
    return 0.0f;
}
} // namespace crisp