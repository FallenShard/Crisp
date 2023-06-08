#include "Normals.hpp"

#include <Crisp/PathTracer/BSDFs/BSDF.hpp>
#include <Crisp/PathTracer/BSSRDFs/BSSRDF.hpp>
#include <Crisp/PathTracer/Core/Intersection.hpp>
#include <Crisp/PathTracer/Core/Scene.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>
#include <Crisp/PathTracer/Shapes/Shape.hpp>

namespace crisp
{
NormalsIntegrator::NormalsIntegrator(const VariantMap& /*params*/) {}

NormalsIntegrator::~NormalsIntegrator() {}

void NormalsIntegrator::preprocess(pt::Scene* /*scene*/) {}

Spectrum NormalsIntegrator::Li(
    const pt::Scene* scene, Sampler& /*sampler*/, Ray3& ray, IlluminationFlags /*illumFlags*/) const
{
    Intersection its;
    if (!scene->rayIntersect(ray, its))
        return Spectrum(0.0f);

    Spectrum color;
    color.r = std::fabsf(its.shFrame.n.x);
    color.g = std::fabsf(its.shFrame.n.y);
    color.b = std::fabsf(its.shFrame.n.z);

    return color;
}
} // namespace crisp