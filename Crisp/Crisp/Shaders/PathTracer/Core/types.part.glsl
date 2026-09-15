#ifndef CRISP_PATH_TRACER_TYPES_GLSL
#define CRISP_PATH_TRACER_TYPES_GLSL

#include "../../Common/pbr-material.part.glsl"
#include "../Lights/light-types.part.glsl"
#include "hit-info.part.glsl"
#include "sample-dimensions.part.glsl"

const float kVacuumIor = 1.0f;

const int kBsdfLambertian = 0;
const int kBsdfDielectric = 1;
const int kBsdfMirror = 2;
const int kBsdfMicrofacet = 3;
const int kBsdfOrenNayar = 4;
const int kBsdfSmoothConductor = 5;
const int kBsdfRoughConductor = 6;
const int kBsdfRoughDielectric = 7;
// One complex, layered type rather than one lobe. Must match kBsdfOpenPbr in Materials/PbrMaterial.hpp.
const int kBsdfOpenPbr = 8;

// Scratch record the closest-hit stage hands to the switches in BSDFs/. It stopped being a callable payload
// when callable dispatch was retired, so its layout is no longer an ABI.
struct BsdfSample {
    vec2 unitSample;  // In, samples a direction or microfacet normal.
    float lobeSample; // In, independently selects a BSDF lobe.
    vec2 texCoord;    // In.
    vec3 wi;          // In, local space.

    vec3 wo;       // Out, sampled direction in local space.
    vec3 f;        // Out, BSDF(wi, wo) * abs(dot(n, wo)).
    float pdf;     // Out.
    vec3 weight;   // Out, f / pdf.
    uint lobeType; // Out, diffuse, glossy, or delta.
};

struct BsdfEval {
    vec3 f; // BSDF(wi, wo) * abs(dot(n, wo)).
    float pdf;
};

#endif // CRISP_PATH_TRACER_TYPES_GLSL
