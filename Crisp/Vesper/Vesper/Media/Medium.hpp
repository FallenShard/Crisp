#pragma once

#include <glm/glm.hpp>

#include "Math/Ray.hpp"
#include "Spectrums/Spectrum.hpp"

#include "Core/VariantMap.hpp"

namespace vesper
{
    class Sampler;
    class Shape;
    class PhaseFunction;

    class Medium
    {
    public:
        struct Sample
        {
            glm::vec3 ref;
            float t;

            float pdfSuccess;
            float pdfFailure;
            glm::vec3 orientation;

            Spectrum sigmaA;
            Spectrum sigmaS;
            Spectrum transmittance;

            const Medium* medium;
        };

        Medium();
        virtual ~Medium();

        virtual void eval(const Ray3& ray, Sample& sample) const = 0;
        virtual Spectrum evalTransmittance(const Ray3& ray, Sampler& sampler) const = 0;
        virtual bool sampleDistance(const Ray3& ray, Sample& sample, Sampler& sampler) const = 0;
        virtual bool isHomogeneous() const = 0;

        void setShape(Shape* shape);

        void setPhaseFunction(PhaseFunction* phaseFunction);
        PhaseFunction* getPhaseFunction() const;

    protected:
        Shape* m_shape;

        PhaseFunction* m_phaseFunction;
        Spectrum m_sigmaA;
        Spectrum m_sigmaS;
        Spectrum m_sigmaT;
    };
}