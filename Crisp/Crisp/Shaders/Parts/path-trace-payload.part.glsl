#ifndef PATH_TRACER_PAYLOAD_PART_GLSL
#define PATH_TRACER_PAYLOAD_PART_GLSL

const int kLobeTypeDiffuse = 1 << 0;
const int kLobeTypeDelta   = 1 << 1;
const int kLobeTypeGlossy  = 1 << 2;

// This structure is used to communicate hit information across path tracing shaders.
struct HitInfo {
    vec3 position;        // Out.
    float tHit;           // Out.

    vec3 sampleDirection; // Out.
    float samplePdf;      // Out.

    vec3 Le;              // Out.
    int lightId;          // Out.

    vec3 bsdfEval;        // Out.
    uint rngSeed;         // In/out.

    vec3 normal;          // Out.
    uint sampleLobeType;  // Out.
};

// This structure is used to communicate BRDF sampling across hit and callable shaders.
struct BrdfSample {
    vec2 unitSample;      // In.
    vec2 pad0;            // Unused.

    vec3 normal;          // In, local space.
    uint materialId;      // In.

    vec3 wi;              // In, local space.
    float pad2;           // Unused.

    vec3 wo;              // Out, local space.
    float pdf;            // Out.

    vec3 f;               // Out, eval * dot(n, wo) / pdf.
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

struct LightParameters {
    int type;
    int meshId;
    int pad0;
    int pad1;
    vec3 radiance;
    float pad2;
};

#endif