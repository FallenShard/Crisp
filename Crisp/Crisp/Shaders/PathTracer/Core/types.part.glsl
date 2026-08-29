#ifndef CRISP_PATH_TRACER_TYPES_GLSL
#define CRISP_PATH_TRACER_TYPES_GLSL

const int kLobeTypeDiffuse = 1 << 0;
const int kLobeTypeDelta = 1 << 1;
const int kLobeTypeGlossy = 1 << 2;

const int kBrdfLambertian = 0;
const int kBrdfDielectric = 1;
const int kBrdfMirror = 2;
const int kBrdfMicrofacet = 3;
const int kBrdfOrenNayar = 4;
const int kBrdfSmoothConductor = 5;

const uint kBrdfOperationSample = 0;
const uint kBrdfOperationEvaluate = 1;

// Fixed layout of the sample vector. Every bounce restarts the sampler cursor at
// kDimBounceBase + bounce * kDimsPerBounce, so a path that skips light sampling on a delta bounce
// or has not reached the Russian-roulette cutoff still consumes the same dimensions as one that
// does. A free-running cursor would let neighbouring pixels disagree on what dimension d means.
const uint kDimPixelFilter = 0u; // 2 dimensions.
const uint kDimBounceBase = 2u;
const uint kDimsPerBounce = 8u;
const uint kDimBsdf = 0u;            // 2 dimensions, relative to the bounce base.
const uint kDimLight = 2u;           // 5 dimensions.
const uint kDimRussianRoulette = 7u; // 1 dimension.

// This structure is used to communicate hit information across path tracing shaders.
struct HitInfo {
    vec3 position; // Out.
    float tHit;    // Out.

    vec3 sampleDirection; // Out.
    float samplePdf;      // Out.

    vec3 Le;     // Out.
    int lightId; // Out.

    vec3 sampleWeight; // Out, sampled f / pdf.
    uint materialId;   // Out.

    vec3 normal;         // Out.
    uint sampleLobeType; // Out.

    vec2 bsdfSample; // In, the unit-square sample the hit shader hands to the BSDF.
};

// This structure is used to communicate BRDF sampling across hit and callable shaders.
struct BrdfSample {
    vec2 unitSample; // In.
    vec2 pad0;       // Unused.

    vec3 normal;     // In, local space.
    uint materialId; // In.

    vec3 wi;        // In, local space.
    uint operation; // In, sample or evaluate.

    vec3 f;    // Out, eval(wi, wo) * abs(dot(n, wo)).
    float pdf; // Out.

    vec3 wo;       // In for evaluation, out for sampling; local space.
    uint lobeType; // Out, diffuse or specular.
};

struct InstanceProperties {
    int materialId;
    int lightId;
    uint vertexOffset;
    uint indexOffset;
    uint aliasTableOffset;
    uint aliasTableCount;
    uint pad0;
    uint pad1;
};

struct BrdfParameters {
    vec3 albedo;
    int type;

    float intIor;
    float extIor;
    int lobe;
    int microfacetType;

    vec3 kd;
    float ks;

    vec3 complexIorEta;
    float microfacetAlpha;

    vec3 complexIorK;
    float roughness;
};

struct BrdfEval {
    vec3 f; // eval(wi, wo) * abs(dot(n, wo)).
    float pdf;
};

struct LightParameters {
    int type;
    int meshId;
    int pad0;
    int pad1;
    vec3 radiance;
    float pad2;
};

#endif // CRISP_PATH_TRACER_TYPES_GLSL
