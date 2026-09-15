#ifndef CRISP_OPENPBR_SURFACE_PARAMS_GLSL
#define CRISP_OPENPBR_SURFACE_PARAMS_GLSL

// The supported opaque subset of OpenPBR Surface 1.1.1, shared by the rasterizer and both path tracers.
// Must match OpenPbrSurfaceParams in Materials/OpenPbrSurface.hpp, which carries the offset assertions.
//
// Every vec3 is followed by a float that fills its padding, which makes this block 64 bytes under scalar and
// std140/std430 alike. That is load-bearing, not cosmetic: the rasterizer reads it through an std430 buffer
// reference and the path tracers read it through a scalar one. Reordering so that two vec3s become adjacent,
// or so that a vec3 lands last, splits the two layouts and silently misreads every record but the first.
struct OpenPbrSurfaceParams {
    vec3 baseColor;
    float baseWeight;

    vec3 specularColor;
    float specularWeight;

    vec3 emissionColor;
    float emissionLuminance;

    float baseMetalness;
    float baseDiffuseRoughness;
    float specularRoughness;
    float specularIor;
};

/*
 * Complete OpenPBR Surface 1.1.1 parameter vocabulary. This is a documentation-only extension of the
 * supported struct above; fields must move into the live CPU/GPU ABI together when their lobes are implemented.
 * The names are the camelCase equivalents of the specification's snake_case identifiers.
 *
 * struct OpenPbrSurfaceParams {
 *     // Base
 *     float baseWeight;
 *     vec3 baseColor;
 *     float baseMetalness;
 *     float baseDiffuseRoughness;
 *
 *     // Specular
 *     float specularWeight;
 *     vec3 specularColor;
 *     float specularRoughness;
 *     float specularRoughnessAnisotropy;
 *     float specularIor;
 *
 *     // Transmission
 *     float transmissionWeight;
 *     vec3 transmissionColor;
 *     float transmissionDepth;
 *     vec3 transmissionScatter;
 *     float transmissionScatterAnisotropy;
 *     float transmissionDispersionScale;
 *     float transmissionDispersionAbbeNumber;
 *
 *     // Subsurface
 *     float subsurfaceWeight;
 *     vec3 subsurfaceColor;
 *     float subsurfaceRadius;
 *     vec3 subsurfaceRadiusScale;
 *     float subsurfaceScatterAnisotropy;
 *
 *     // Coat
 *     float coatWeight;
 *     vec3 coatColor;
 *     float coatRoughness;
 *     float coatRoughnessAnisotropy;
 *     float coatIor;
 *     float coatDarkening;
 *
 *     // Fuzz
 *     float fuzzWeight;
 *     vec3 fuzzColor;
 *     float fuzzRoughness;
 *
 *     // Emission
 *     float emissionLuminance;
 *     vec3 emissionColor;
 *
 *     // Thin film
 *     float thinFilmWeight;
 *     float thinFilmThickness;
 *     float thinFilmIor;
 *
 *     // Geometry
 *     float geometryOpacity;
 *     bool geometryThinWalled;
 *     vec3 geometryNormal;
 *     vec3 geometryTangent;
 *     vec3 geometryCoatNormal;
 *     vec3 geometryCoatTangent;
 * };
 */

// Normal-incidence reflectance of the dielectric specular lobe. Lives with the parameters rather than with
// either evaluator: the rasteriser and the path tracer both need it, and they have to agree at the endpoints
// or the same material reads as two different dielectrics.
float openPbrDielectricF0(const float ior, const float weight) {
    const float eta = max(ior, 0.001f);
    const float unweighted = pow((1.0f - eta) / (1.0f + eta), 2.0f);
    return clamp(max(weight, 0.0f) * unweighted, 0.0f, 0.9999f);
}

#endif // CRISP_OPENPBR_SURFACE_PARAMS_GLSL
