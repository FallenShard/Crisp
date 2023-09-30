#include "AmbientOcclusion.hpp"

#include <Crisp/Math/Warp.hpp>
#include <Crisp/PathTracer/BSDFs/BSDF.hpp>
#include <Crisp/PathTracer/BSSRDFs/BSSRDF.hpp>
#include <Crisp/PathTracer/Core/Intersection.hpp>
#include <Crisp/PathTracer/Core/Scene.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>
#include <Crisp/PathTracer/Shapes/Shape.hpp>

namespace crisp {
AmbientOcclusionIntegrator::AmbientOcclusionIntegrator(const VariantMap& params) {
    m_rayLength = params.get<float>("rayLength", 1.0f);
}

AmbientOcclusionIntegrator::~AmbientOcclusionIntegrator() {}

void AmbientOcclusionIntegrator::preprocess(pt::Scene* /*scene*/) {}

Spectrum AmbientOcclusionIntegrator::Li(
    const pt::Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags /*illumFlags*/) const {
    Intersection its;
    if (!scene->rayIntersect(ray, its)) {
        return Spectrum(1.0f);
    }

    glm::vec3 hemisphereDir = warp::squareToUniformHemisphere(sampler.next2D());

    Ray3 testRay(its.p, its.toWorld(hemisphereDir), Ray3::Epsilon, m_rayLength);

    if (scene->rayIntersect(testRay)) {
        return Spectrum(0.0f);
    }

    return Spectrum(1.0f);
}
} // namespace crisp