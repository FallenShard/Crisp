#ifndef PATH_TRACER_PAYLOAD_PART_GLSL
#define PATH_TRACER_PAYLOAD_PART_GLSL

const int kLobeDelta = 0;
const int kLobeGlossy = 1;

struct HitInfo
{
    vec3 position;
    float tHit;

    vec3 sampleDirection;
    float samplePdf;

    vec3 Le;
    uint bounceCount;

    vec3 bsdfEval;
    uint rngSeed;

    vec3 debugValue;
    int sampleLobeType;
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

#endif