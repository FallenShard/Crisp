#include "EnvironmentLight.hpp"

#include <glm/gtx/norm.hpp>

#include "Math/Operations.hpp"
#include "Math/DiscretePdf.hpp"
#include "Math/Warp.hpp"

#include "Shapes/Shape.hpp"
#include "Samplers/Sampler.hpp"

#include "Core/MipMap.hpp"

namespace vesper
{
    EnvironmentLight::EnvironmentLight(const VariantMap& params)
    {
        auto probeFilename = params.get<std::string>("fileName", "uffizi-large.exr");
        m_scale = params.get<float>("scale", 1.0f);


        m_probe = std::make_unique<MipMap<Spectrum>>(probeFilename);

        int h = m_probe->getHeight();
        int w = m_probe->getWidth();

        m_phiPdfs.resize(h);
        m_thetaPdf.reserve(h);

        for (int i = 0; i < h; i++)
        {
            float theta = i / static_cast<float>(h) * PI;
            float sinTheta = std::sinf(theta);

            auto& phiPdf = m_phiPdfs[i];
            phiPdf.reserve(w);

            for (int j = 0; j < w; j++)
            {
                float scalar = m_probe->fetch(j, i).getLuminance() * sinTheta;
                phiPdf.append(scalar);
            }

            float sum = phiPdf.normalize();
            m_thetaPdf.append(sum);
        }

        m_thetaPdf.normalize();
    }

    EnvironmentLight::~EnvironmentLight()
    {
    }

    Spectrum EnvironmentLight::eval(const Light::Sample& sample) const
    {
        float u = (1.0f + atan2(sample.wi.x, -sample.wi.z) * InvPI) * 0.5f;
        float v = acosf(sample.wi.y) * InvPI;

        return m_scale * m_probe->evalLerp(u, v);
    }

    Spectrum EnvironmentLight::sample(Light::Sample& sample, Sampler& sampler) const
    {
        auto point = sampler.next2D();

        auto thetaIdx = m_thetaPdf.sample(point.y);
        auto phiIdx = m_phiPdfs[thetaIdx].sample(point.x);
        
        auto v = static_cast<float>(thetaIdx) / m_probe->getHeight();
        auto u = 2.0f * static_cast<float>(phiIdx) / m_probe->getWidth();
        auto radiance = m_probe->fetch(static_cast<int>(phiIdx), static_cast<int>(thetaIdx));
        
        auto theta = PI * v;
        auto phi = PI * (u - 1.0f);
        
        sample.wi.x = sinf(theta) * sinf(phi);
        sample.wi.y = cosf(theta);
        sample.wi.z = -sinf(theta) * cosf(phi);
        sample.pdf = m_thetaPdf[thetaIdx] * m_phiPdfs[thetaIdx][phiIdx] * InvTwoPI * InvPI / sinf(theta);

        // Uniform
        //sample.wi = Warp::squareToUniformSphere(point);
        //sample.pdf = Warp::squareToUniformSpherePdf();

        sample.shadowRay = Ray3(sample.ref, sample.wi, Ray3::Epsilon, m_sceneRadius * 2.0f);

        return m_scale * radiance / sample.pdf;
    }

    float EnvironmentLight::pdf(const Light::Sample& sample) const
    {
        float theta = acosf(sample.wi.y);
        float u = (1.0f + atan2(sample.wi.x, -sample.wi.z) * InvPI) * 0.5f;
        float v = theta * InvPI;
        
        auto vSample = m_thetaPdf.sample(v);
        auto uSample = m_phiPdfs[vSample].sample(u);
        
        return m_thetaPdf[vSample] * m_phiPdfs[vSample][uSample] * InvTwoPI * InvPI / sinf(theta);
        //return Warp::squareToUniformSpherePdf();
    }

    Spectrum EnvironmentLight::samplePhoton(Ray3& ray, Sampler& sampler) const
    {
        return Spectrum();
    }

    void EnvironmentLight::setBoundingSphere(const glm::vec4& sphereParams)
    {
        m_sceneCenter.x = sphereParams.x;
        m_sceneCenter.y = sphereParams.y;
        m_sceneCenter.z = sphereParams.z;
        m_sceneRadius = sphereParams.w;
    }
}