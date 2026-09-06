#ifndef CRISP_PATH_TRACER_PBR_HIT_GLSL
#define CRISP_PATH_TRACER_PBR_HIT_GLSL

// Payload shared by the raygen, closest-hit and miss stages of the PBR path-traced view. The hit shader samples
// the BSDF and reports the surface it sampled; next-event estimation runs in the raygen, which rebuilds that
// same surface from these fields. MIS is only valid when the two agree, so anything that moves the shading
// frame -- normal mapping in particular -- has to travel here rather than be recomputed.
struct PbrHitInfo {
    vec3 position; // Out.
    float tHit;    // Out, negative on a miss.

    vec3 sampleDirection; // Out, world space.
    float samplePdf;      // Out.

    vec3 sampleWeight; // Out, f * cos(wo) / pdf.
    uint sampleIsSpecular;

    vec3 emission;      // Out, environment radiance on a miss.
    uint materialIndex; // Out.

    vec3 normal;                // Out, world-space shading normal, after normal mapping.
    uint materialTextureOffset; // Out, kInvalidMaterialTextureOffset when the material is untextured.

    vec2 unitSample; // In, the BSDF sample the raygen hands to the hit shader.
    vec2 texCoord;   // Out, unscaled; applyMaterialTextures applies the material's uvScale.
};

#endif // CRISP_PATH_TRACER_PBR_HIT_GLSL
