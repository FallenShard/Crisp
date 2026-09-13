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

#endif // CRISP_OPENPBR_SURFACE_PARAMS_GLSL
