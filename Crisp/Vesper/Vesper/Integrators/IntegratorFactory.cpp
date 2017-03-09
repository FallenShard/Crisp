#include "IntegratorFactory.hpp"

#include "Normals.hpp"
#include "AmbientOcclusion.hpp"
#include "DirectLighting.hpp"
#include "PathTracer.hpp"
#include "MisPathTracer.hpp"
#include "MisDirectLighting.hpp"

namespace vesper
{
    std::unique_ptr<Integrator> IntegratorFactory::create(std::string type, VariantMap parameters)
    {
        if (type == "normals")
        {
            return std::make_unique<NormalsIntegrator>(parameters);
        }
        else if (type == "ambient-occlusion")
        {
            return std::make_unique<AmbientOcclusionIntegrator>(parameters);
        }
        else if (type == "direct-lighting")
        {
            return std::make_unique<DirectLightingIntegrator>(parameters);
        }
        else if (type == "mis-direct-lighting")
        {
            return std::make_unique<MisDirectLightingIntegrator>(parameters);
        }
        else if (type == "path-tracer")
        {
            return std::make_unique<PathTracerIntegrator>(parameters);
        }
        else if (type == "mis-path-tracer")
        {
            return std::make_unique<MisPathTracerIntegrator>(parameters);
        }
        else
        {
            std::cerr << "Unknown integrator type \"" << type << "\" requested! Creating default normals integrator" << std::endl;
            return std::make_unique<NormalsIntegrator>(parameters);
        }
    }
}