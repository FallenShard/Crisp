#include "Normals.hpp"

#include "Core/Intersection.hpp"
#include "Core/Scene.hpp"

namespace crisp
{
    NormalsIntegrator::NormalsIntegrator(const VariantMap& /*params*/)
    {
    }

    NormalsIntegrator::~NormalsIntegrator()
    {
    }

    void NormalsIntegrator::preprocess(Scene* /*scene*/)
    {
    }

    Spectrum NormalsIntegrator::Li(const Scene* scene, Sampler& /*sampler*/, Ray3& ray, IlluminationFlags /*illumFlags*/) const
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
}