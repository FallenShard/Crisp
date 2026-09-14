#ifndef CRISP_PATH_TRACER_HIT_INFO_GLSL
#define CRISP_PATH_TRACER_HIT_INFO_GLSL

const int kLobeTypeDiffuse = 1 << 0;
const int kLobeTypeDelta = 1 << 1;
const int kLobeTypeGlossy = 1 << 2;

// The ray payload every path tracer shares. The closest-hit shader samples the BSDF and reports the surface it
// sampled; next-event estimation runs in the raygen, which rebuilds that same surface from these fields. MIS is
// only valid when the two agree, so anything that moves the shading frame -- normal mapping in particular --
// has to travel here rather than be recomputed.
//
// It lives apart from types.part.glsl so a stage can take the payload without the material and light records,
// and without the analytic tracer's kDimsPerBounce.
struct HitInfo {
    vec3 position; // Out.
    float tHit;    // Out, negative on a miss.

    vec3 sampleDirection; // Out, world space.
    float samplePdf;      // Out.

    vec3 sampleWeight;   // Out, f * cos(wo) / pdf.
    uint sampleLobeType; // Out, diffuse, glossy, or delta.

    vec3 Le;         // Out, surface emission; undefined on a miss.
    uint materialId; // Out.

    vec3 normal;                // Out, world-space shading normal, after normal mapping.
    uint materialTextureOffset; // Out, kInvalidMaterialTextureOffset when the material is untextured.

    vec2 bsdfSample;      // In, the unit-square sample the hit shader hands to the BSDF.
    float bsdfLobeSample; // In, independent sample for selecting a BSDF lobe.
    int lightId;          // Out, -1 when the hit is not an emitter.

    vec2 texCoord; // Out, unscaled; applyMaterialTextures applies the material's uvScale.
    vec2 pad0;
};

#endif // CRISP_PATH_TRACER_HIT_INFO_GLSL
