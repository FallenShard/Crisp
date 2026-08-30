#ifndef CRISP_PATH_TRACER_PBR_HIT_GLSL
#define CRISP_PATH_TRACER_PBR_HIT_GLSL

// Payload shared by the raygen, closest-hit and miss stages of the PBR path-traced view.
struct PbrHitInfo {
    vec3 position; // Out.
    float tHit;    // Out, negative on a miss.

    vec3 sampleDirection; // Out, world space.
    float samplePdf;      // Out.

    vec3 sampleWeight; // Out, f * cos(wo) / pdf.
    uint sampleIsSpecular;

    vec3 emission; // Out, environment radiance on a miss.
    uint pad0;

    vec2 unitSample; // In, the BSDF sample the raygen hands to the hit shader.
    vec2 pad1;
};

#endif // CRISP_PATH_TRACER_PBR_HIT_GLSL
