#ifndef CRISP_PATH_TRACER_TYPES_GLSL
#define CRISP_PATH_TRACER_TYPES_GLSL

#include "../../Common/openpbr-surface.part.glsl"
#include "hit-info.part.glsl"

const float kVacuumIor = 1.0f;

const int kBsdfLambertian = 0;
const int kBsdfDielectric = 1;
const int kBsdfMirror = 2;
const int kBsdfMicrofacet = 3;
const int kBsdfOrenNayar = 4;
const int kBsdfSmoothConductor = 5;
const int kBsdfRoughConductor = 6;
const int kBsdfRoughDielectric = 7;
// One complex, layered type rather than one lobe. Must match kBsdfOpenPbr in Scenes/RayTracingSceneData.hpp,
// whose ordering also fixes the callable shader binding table.
const int kBsdfOpenPbr = 8;

const int kLightArea = 0;
const int kLightPoint = 1;
const int kLightDirectional = 2;

// Fixed layout of the sample vector. Every bounce restarts the sampler cursor at
// kDimBounceBase + bounce * kDimsPerBounce, so a path that skips light sampling on a delta bounce
// or has not reached the Russian-roulette cutoff still consumes the same dimensions as one that
// does. A free-running cursor would let neighbouring pixels disagree on what dimension d means.
const uint kDimPixelFilter = 0u; // 2 dimensions.
const uint kDimBounceBase = 2u;
const uint kDimsPerBounce = 9u;
const uint kDimBsdf = 0u;            // 3 dimensions, relative to the bounce base.
const uint kDimLight = 3u;           // 5 dimensions.
const uint kDimRussianRoulette = 8u; // 1 dimension.

// Callable payload shared by the closest-hit shader and every BSDF callable.
struct BsdfSample {
    vec2 unitSample;  // In, samples a direction or microfacet normal.
    float lobeSample; // In, independently selects a BSDF lobe.
    uint materialId;  // In.

    vec3 wi; // In, local space.

    vec3 f;    // Out, BSDF(wi, wo) * abs(dot(n, wo)).
    float pdf; // Out.

    vec3 wo;       // Out, sampled direction in local space.
    uint lobeType; // Out, diffuse, glossy, or delta.

    vec2 texCoord; // In.
};

// Must match BsdfParameters in Scenes/RayTracingSceneParser.hpp. Parameters with an exact OpenPBR equivalent
// use the canonical surface block even for legacy BSDF types; the trailing fields are legacy-only values and
// texture metadata.
struct BsdfParameters {
    OpenPbrSurfaceParams surface;

    vec3 complexIorEta;
    float microfacetAlpha;

    vec3 complexIorK;
    float orenNayarRoughness;

    int type;
    int microfacetType;
    int reflectanceTexture;
    int reflectanceSampler;
};

struct BsdfEval {
    vec3 f; // BSDF(wi, wo) * abs(dot(n, wo)).
    float pdf;
};

struct LightParameters {
    int type;
    int meshId;
    int pad0;
    int pad1;
    vec3 emission; // Area radiance, point power, or directional irradiance.
    float pad2;
    vec3 positionOrDirection;
    float pad3;
};

#endif // CRISP_PATH_TRACER_TYPES_GLSL
