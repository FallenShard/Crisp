#ifndef CRISP_PATH_TRACER_INTEGRATOR_GLSL
#define CRISP_PATH_TRACER_INTEGRATOR_GLSL

#include "heap-slots.part.glsl"

// Must match PathTracedIntegratorParams in Scenes/PathTracer.hpp. A heap array may be declared only once per
// stage, so the block lives here rather than in each raygen.
layout(descriptor_heap, descriptor_stride = 64) uniform IntegratorParams {
    int maxBounces;
    int sampleCount;
    int frameIdx;
    int sampleOffset;

    uint seed;
    int reconstructionFilter;
    int lightCount;
    int shapeCount;

    int samplingMode;
    int environmentEnabled;
    int environmentWidth;
    int environmentHeight;

    float environmentIntensity;
    uint visibilityMask;
    uint pad0;
    uint pad1;
} heapIntegrators[];

#define integrator heapIntegrators[kIntegratorSlot]

#endif // CRISP_PATH_TRACER_INTEGRATOR_GLSL
