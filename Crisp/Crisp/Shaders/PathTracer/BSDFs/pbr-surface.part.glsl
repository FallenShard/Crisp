#ifndef CRISP_PBR_SURFACE_GLSL
#define CRISP_PBR_SURFACE_GLSL

#include "../../Brdf/lambertian.part.glsl"
#include "../../Brdf/microfacet.part.glsl"
#include "../../Brdf/OpenPbr/energy-compensation.part.glsl"
#include "../../Common/math-constants.part.glsl"

// Stand-in that deliberately mirrors Shaders/pbr.frag.glsl rather than being correct on its own terms.
// Replaced wholesale by Brdf/OpenPbr; see docs/openpbr-path-tracer.md.

float dielectricF0(const float ior, const float weight) {
    const float eta = max(ior, 0.001f);
    const float unweighted = pow((1.0f - eta) / (1.0f + eta), 2.0f);
    return clamp(max(weight, 0.0f) * unweighted, 0.0f, 0.9999f);
}

struct PbrSurface {
    vec3 diffuseAlbedo;
    vec3 f0;
    float alpha;
    float specularProbability;
    uint energyCompensation;
};

float luminance(const vec3 value) {
    return dot(value, vec3(0.2126f, 0.7152f, 0.0722f));
}

PbrSurface createPbrSurface(const PbrMaterialParameters material, const uint energyCompensation) {
    const float metalness = clamp(material.baseMetalness, 0.0f, 1.0f);
    const vec3 baseColor = max(material.baseColor, vec3(0.0f)) * max(material.baseWeight, 0.0f);
    const vec3 dielectric =
        vec3(dielectricF0(material.specularIor, material.specularWeight)) * clamp(material.specularColor, 0.0f, 1.0f);

    PbrSurface surface;
    surface.diffuseAlbedo = baseColor * (1.0f - metalness);
    surface.f0 = mix(dielectric, baseColor, metalness);
    const float roughness = clamp(material.specularRoughness, 1e-3f, 1.0f);
    surface.alpha = roughness * roughness;
    surface.energyCompensation = energyCompensation;

    // Selecting the lobe by its approximate share of the reflected energy keeps a black dielectric from
    // spending every sample on a diffuse lobe that returns nothing.
    const float diffuseWeight = luminance(surface.diffuseAlbedo);
    const float specularWeight = luminance(surface.f0);
    const float total = diffuseWeight + specularWeight;
    surface.specularProbability = total > 0.0f ? clamp(specularWeight / total, 0.1f, 0.9f) : 1.0f;
    return surface;
}

vec3 evaluatePbrSurface(const PbrSurface surface, const vec3 wi, const vec3 wo) {
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return vec3(0.0f);
    }

    const vec3 halfVector = normalize(wi + wo);
    const vec3 fresnel = fresnelSchlick(max(dot(wi, halfVector), 0.0f), surface.f0);

    const vec3 diffuse = (1.0f - fresnel) * surface.diffuseAlbedo / PI;
    const float distribution = ggxDistribution(halfVector, surface.alpha);
    const float geometry = ggxGeometry(wi, wo, halfVector, surface.alpha);
    vec3 specular = fresnel * distribution * geometry / (4.0f * wi.z * wo.z);

    if (surface.energyCompensation == kEnergyCompensationTurquin) {
        specular *= turquinScale(wi.z, surface.alpha, schlickFresnelAverage(surface.f0));
    } else if (surface.energyCompensation == kEnergyCompensationKullaConty) {
        specular += kullaContyLobe(wi.z, wo.z, surface.alpha, schlickFresnelAverage(surface.f0));
    }

    return (diffuse + specular) * wo.z;
}

float pbrSurfacePdf(const PbrSurface surface, const vec3 wi, const vec3 wo) {
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return 0.0f;
    }

    const vec3 halfVector = normalize(wi + wo);
    const float specularPdf =
        microfacetNormalPdf(halfVector, kMicrofacetGgx, surface.alpha) / (4.0f * max(dot(halfVector, wo), 1e-6f));
    const float diffusePdf = wo.z / PI;
    return mix(diffusePdf, specularPdf, surface.specularProbability);
}

// Returns f * cos(wo) / pdf for the sampled direction, or zero when the sample leaves the upper hemisphere.
vec3 samplePbrSurface(
    const PbrSurface surface, vec2 unitSample, const vec3 wi, out vec3 wo, out float pdf, out bool sampledSpecular) {
    sampledSpecular = unitSample.x < surface.specularProbability;
    if (sampledSpecular) {
        unitSample.x /= surface.specularProbability;
        const vec3 microfacetNormal = sampleGGXNormal(unitSample, surface.alpha);
        wo = 2.0f * dot(microfacetNormal, wi) * microfacetNormal - wi;
    } else {
        unitSample.x = (unitSample.x - surface.specularProbability) / (1.0f - surface.specularProbability);
        wo = squareToCosineHemisphere(unitSample);
    }

    pdf = pbrSurfacePdf(surface, wi, wo);
    if (pdf <= 0.0f || wo.z <= 0.0f) {
        pdf = 0.0f;
        return vec3(0.0f);
    }

    return evaluatePbrSurface(surface, wi, wo) / pdf;
}

#endif // CRISP_PBR_SURFACE_GLSL
