#pragma once

#include <Crisp/Math/Ray.hpp>
#include <Crisp/PathTracer/Core/VariantMap.hpp>
#include <Crisp/PathTracer/Spectra/Spectrum.hpp>

namespace crisp {
class Sampler;
class Shape;

class Light {
public:
    struct Sample {
        glm::vec3 ref; // In(eval, sample) - Vantage point that is being illuminated
        glm::vec3 p;   // In(eval), Out(sample) - Point on the emitter
        glm::vec3 n;   // In(eval), Out(sample) - Normal at the emitter point
        glm::vec3 wi;  // In(eval), Out(sample) - Vantage-to-emitter direction

        float pdf; // In(eval), Out(sample) - Probability of selecting sampled emitter point

        Ray3 shadowRay; // Out(sample) - Accompanying shadow ray

        Light* light;

        Sample()
            : pdf(0.0f)
            , light(nullptr) {}

        // To be used when sampling the emitter
        Sample(const glm::vec3& ref)
            : ref(ref)
            , pdf(0.0f)
            , light(nullptr) {}

        // To be used when evaluating the emitter
        Sample(const glm::vec3& ref, const glm::vec3& p, const glm::vec3& n)
            : ref(ref)
            , p(p)
            , n(n)
            , pdf(0.0f)
            , light(nullptr) {}
    };

    virtual ~Light() {};

    virtual bool isOnSurface() {
        return true;
    }

    void setShape(Shape* shape) {
        m_shape = shape;
    }

    virtual void setBoundingSphere(const glm::vec4& /*sphereParams*/) {}

    // Evaluates emitted radiance Le
    virtual Spectrum eval(const Light::Sample& sample) const = 0;

    // Computes emitted radiance divided by probability density -> Le / pdf
    virtual Spectrum sample(Light::Sample& sample, Sampler& sampler) const = 0;
    virtual float pdf(const Light::Sample& sample) const = 0;

    virtual Spectrum samplePhoton(Ray3& ray, Sampler& sampler) const = 0;
    virtual bool isDelta() const = 0;

protected:
    Shape* m_shape;
};
} // namespace crisp