#ifndef CRISP_PATH_TRACE_PAYLOAD_GLSL
#define CRISP_PATH_TRACE_PAYLOAD_GLSL

const int kLobeTypeDiffuse = 1 << 0;
const int kLobeTypeDelta   = 1 << 1;
const int kLobeTypeGlossy  = 1 << 2;

const int kBrdfLambertian = 0;
const int kBrdfDielectric = 1;
const int kBrdfMirror = 2;
const int kBrdfMicrofacet = 3;

const uint kBrdfOperationSample = 0;
const uint kBrdfOperationEvaluate = 1;

// This structure is used to communicate hit information across path tracing shaders.
struct HitInfo {
    vec3 position;         // Out.
    float tHit;            // Out.

    vec3 sampleDirection;  // Out.
    float samplePdf;       // Out.

    vec3 Le;               // Out.
    int lightId;           // Out.

    vec3 sampleWeight;     // Out, sampled f / pdf.
    uint rngSeed;          // In/out.

    vec3 normal;           // Out.
    uint sampleLobeType;   // Out.

    uint materialId;       // Out.
};

// This structure is used to communicate BRDF sampling across hit and callable shaders.
struct BrdfSample {
    vec2 unitSample;      // In.
    vec2 pad0;            // Unused.

    vec3 normal;          // In, local space.
    uint materialId;      // In.

    vec3 wi;              // In, local space.
    uint operation;       // In, sample or evaluate.

    vec3 f;               // Out, eval(wi, wo) * abs(dot(n, wo)).
    float pdf;            // Out.

    vec3 wo;              // In for evaluation, out for sampling; local space.
    uint lobeType;        // Out, diffuse or specular.
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
    float pad1;
};

struct BrdfEval {
    vec3 f;    // eval(wi, wo) * abs(dot(n, wo)).
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

#endif // CRISP_PATH_TRACE_PAYLOAD_GLSL
