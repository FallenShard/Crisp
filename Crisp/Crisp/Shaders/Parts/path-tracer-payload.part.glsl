#ifndef PATH_TRACER_PAYLOAD_PART_GLSL
#define PATH_TRACER_PAYLOAD_PART_GLSL

const int kLobeDelta = 0;
const int kLobeGlossy = 1;

struct HitInfo
{
    vec3 position;        // Out.
    float tHit;           // Out.

    vec3 sampleDirection; // Out.
    float samplePdf;      // Out.

    vec3 Le;              // Out.
    uint bounceCount;     // Out, unnecessary.

    vec3 bsdfEval;        // Out.
    uint rngSeed;         // In/out.

    vec3 normal;          // Out.
    int lightId;          // Out.
};

struct ShadowRayHitInfo
{
    vec3 position;
    float tHit;
};

struct BsdfSample
{
    vec2 unitSample;
    vec2 pad0;

    vec3 normal;
    uint materialId;

    vec3 wi;
    float pad2;

    vec3 sampleDirection;
    float samplePdf;

    vec3 eval;
    float pad1;
};

struct BrdfParameters
{
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

struct LightParameters {
    int type;
    int meshId;
    int pad0;
    int pad1;
    vec3 radiance;
    float pad2;
};

#endif