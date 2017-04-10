#pragma once

#include <vector>
#include <memory>

#include "Light.hpp"
#include "Math/Distribution1D.hpp"
#include "Core/MipMap.hpp"

namespace vesper
{
    class EnvironmentLight : public Light
    {
    public:
        EnvironmentLight(const VariantMap& params = VariantMap());
        ~EnvironmentLight();

        virtual Spectrum eval(const Light::Sample& emitterSample) const override;
        virtual Spectrum sample(Light::Sample& emitterSample, Sampler& sampler) const override;
        virtual float pdf(const Light::Sample& emitterSample) const override;

        virtual Spectrum samplePhoton(Ray3& ray, Sampler& sampler) const override;

        virtual void setBoundingSphere(const glm::vec4& sphereParams) override;
        virtual bool isDelta() const override;

    private:
        glm::vec2 sampleContinuous(const glm::vec2& point, float& pdf) const;
        float distributionPdf(float u, float v) const;

        std::unique_ptr<MipMap<Spectrum>> m_probe;
        float m_scale;

        std::vector<float> m_cdfCols;
        std::vector<float> m_cdfRows;

        //std::vector<Distribution1D> m_phiPdfs;
        //Distribution1D m_thetaPdf;
        std::vector<float> m_rowWeights;
        float m_normalization;
        glm::vec2 m_pixelSize;

        Spectrum m_power;

        glm::vec3 m_sceneCenter;
        float m_sceneRadius;
    };
}