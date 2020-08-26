#include "VolumePathTracer.hpp"

#include "Samplers/Sampler.hpp"
#include "Core/Scene.hpp"
#include "BSDFs/BSDF.hpp"
#include "Core/Intersection.hpp"
#include "Shapes/Shape.hpp"
#include "Media/Medium.hpp"
#include "PhaseFunctions/PhaseFunction.hpp"

namespace crisp
{
    namespace
    {
        inline float miWeight(float pdf1, float pdf2)
        {
            pdf1 *= pdf1;
            pdf2 *= pdf2;
            return pdf1 / (pdf1 + pdf2);
        }

        inline const Medium* resolveNextMedium(const Intersection& its, const glm::vec3& dir)
        {
            if (glm::dot(its.geoFrame.n, dir) >= 0.0f)
                return nullptr;
            else
                return its.shape->getMedium();
        }

        Spectrum evalTransmittance(const Scene* scene, const glm::vec3& start, bool startOnSurface, const glm::vec3& end, bool endOnSurface, const Medium* medium, Sampler& sampler)
        {
            glm::vec3 dir = end - start;
            float distance = glm::length(dir);
            dir = glm::normalize(dir);

            float lengthFactor  = endOnSurface   ? (1.0f - Ray3::Epsilon) : 1.0f;
            float minT = startOnSurface ? Ray3::Epsilon : 0.0f;

            Ray3 shadowRay(start, dir, minT, distance * lengthFactor);
            Spectrum transmittance(1.0f);
            Intersection its;

            while (distance > 0.0f)
            {
                bool intersected = scene->rayIntersect(shadowRay, its);
                if (intersected && !(its.shape->getBSDF()->getLobeType() & Lobe::Passthrough))
                    return Spectrum(0.0f);

                if (medium)
                    transmittance *= medium->evalTransmittance(Ray3(shadowRay, 0, std::min(its.tHit, distance)), sampler);

                if (!intersected || transmittance.isZero())
                    break;

                auto bsdf = its.shape->getBSDF();
                glm::vec3 localWi = its.shFrame.toLocal(shadowRay.d);
                BSDF::Sample sample(its.p, its.uv, localWi, -localWi);
                transmittance *= bsdf->eval(sample);

                medium = resolveNextMedium(its, dir);

                distance -= its.tHit;

                shadowRay.o = shadowRay(its.tHit);
                shadowRay.maxT = distance * lengthFactor;
                shadowRay.minT = Ray3::Epsilon;
                shadowRay.update();
            }

            return transmittance;
        }

        Spectrum sampleLightFromMedium(Light::Sample& lightSample, const Scene* scene, const Medium* medium, Sampler& sampler)
        {
            auto light = scene->getRandomLight(sampler.next1D());
            if (!light)
                return Spectrum(0.0f);

            Spectrum lightContrib = light->sample(lightSample, sampler);
            if (lightSample.pdf == 0.0f)
                return Spectrum(0.0f);

            lightContrib *= evalTransmittance(scene, lightSample.ref, false, lightSample.p, true, medium, sampler);
            lightContrib /= scene->getLightPdf();
            lightSample.pdf *= scene->getLightPdf();
            lightSample.light = light;
            return lightContrib;
        }

        Spectrum findLight(const Scene* scene, Sampler& sampler, const Medium* medium, Ray3 ray, Intersection& itsRef, Light::Sample& /*lightSample*/)
        {
            Intersection its2;
            Intersection* its = &itsRef;

            Spectrum transmittance(1.0f);
            bool firstIntersected = false;
            bool hasIntersected = false;

            int i = 0;
            while (true)
            {
                hasIntersected = scene->rayIntersect(ray, *its);
                if (i++ == 0)
                    firstIntersected = hasIntersected;

                if (medium)
                    transmittance *= medium->evalTransmittance(Ray3(ray, 0.0f, its->tHit), sampler);

                if (!hasIntersected)
                    break;

                if (hasIntersected &&
                    !(its->shape->getBSDF()->getLobeType() & Lobe::Passthrough) &&
                    !(its->shape->getLight()))
                    break;

                if (transmittance.isZero())
                    return Spectrum(0.0f);

            }

            return transmittance;
        }
    }

    VolumePathTracerIntegrator::VolumePathTracerIntegrator(const VariantMap& /*attributes*/)
    {
    }

    VolumePathTracerIntegrator::~VolumePathTracerIntegrator()
    {
    }

    void VolumePathTracerIntegrator::preprocess(Scene* /*scene*/)
    {
    }

    Spectrum VolumePathTracerIntegrator::Li(const Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags /*flags*/) const
    {
        Spectrum L(0.0f);

        Spectrum throughput(1.0f);
        //float eta = 1.0f;

        //unsigned int bounces = 0;
        //bool isDeltaBounce = true;
        //bool includeEmitted = true;
        //bool scattered = false;

        Medium* medium = nullptr;
        Medium::Sample mediumSample;

        Intersection its;

        while (true)
        {
            if (medium && medium->sampleDistance(Ray3(ray, 0.0f, its.tHit), mediumSample, sampler))
            {
                auto phaseFunc = medium->getPhaseFunction();

                throughput *= mediumSample.sigmaS * mediumSample.transmittance / mediumSample.pdfSuccess;

                Light::Sample lightSample(mediumSample.ref);
                auto Latt = sampleLightFromMedium(lightSample, scene, medium, sampler);
                if (!Latt.isZero())
                {
                    PhaseFunction::Sample pfSample(mediumSample, -ray.d, lightSample.wi);
                    float phaseFuncValue = phaseFunc->eval(pfSample);

                    if (phaseFuncValue > 0.0f)
                    {
                        float pfPdf = phaseFunc->pdf(pfSample);
                        L += throughput * Latt * phaseFuncValue * miWeight(lightSample.pdf, pfPdf);
                    }
                }

                PhaseFunction::Sample pfSample(mediumSample, -ray.d);
                float phaseFuncValue = phaseFunc->sample(pfSample, sampler);
                if (phaseFuncValue == 0.0f)
                    break;

                throughput *= phaseFuncValue;

                ray = Ray3(mediumSample.ref, pfSample.wo, 0.0f, ray.maxT);

                Latt = findLight(scene, sampler, medium, ray, its, lightSample);
                if (!Latt.isZero())
                {
                    float lightPdf = lightSample.light->pdf(lightSample);
                    L += throughput * Latt * phaseFuncValue * miWeight(pfSample.pdf, lightPdf);
                }
            }
        }

        return L;
    }

}