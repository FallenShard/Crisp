#ifndef CRISP_PATH_TRACER_ENVIRONMENT_CDF_GLSL
#define CRISP_PATH_TRACER_ENVIRONMENT_CDF_GLSL

// The marginal-then-conditional CDF over the equirectangular environment, as built by
// Scenes/EnvironmentLightSampling. Its own header because both the push-constant block and the sampling
// routines need the type, and a buffer reference may only be declared once per stage.
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer EnvironmentCdf {
    float data[];
};

#endif // CRISP_PATH_TRACER_ENVIRONMENT_CDF_GLSL
