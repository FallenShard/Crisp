#ifndef PATH_TRACER_PAYLOAD_PART_GLSL
#define PATH_TRACER_PAYLOAD_PART_GLSL

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

    vec4 debugValue;
};

struct LambertianBsdfSample
{
    vec2 unitSample;
    vec2 pad0;

    vec3 normal;
    float pad1;

    vec3 wi;
    float pad2;

    vec3 sampleDirection;
    float samplePdf;

    vec3 eval;
    float pad3;
};

struct DielectricBsdfSample
{
    vec2 unitSample;
    float intIOR;
    float extIOR;

    vec3 normal;
    float pad1;

    vec3 wi;
    float pad2;

    vec3 sampleDirection;
    float samplePdf;

    vec3 eval;
    float pad3;
};

struct MirrorBsdfSample
{
    vec3 normal;
    float pad1;

    vec3 wi;
    float pad2;

    vec3 sampleDirection;
    float samplePdf;

    vec3 eval;
    float pad3;
};

#endif