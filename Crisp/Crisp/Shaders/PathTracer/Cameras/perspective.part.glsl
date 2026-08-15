#ifndef CRISP_PATH_TRACER_PERSPECTIVE_GLSL
#define CRISP_PATH_TRACER_PERSPECTIVE_GLSL

void sampleRay(out vec4 origin, out vec4 direction, in vec2 pixelSample) {
    const vec2 ndcSample = pixelSample / vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0; // In [-1, 1].

    origin = view.invV * vec4(0, 0, 0, 1);

    const vec4 target = view.invP * vec4(ndcSample, 0, 1);
    const vec3 rayDirEyeSpace = normalize(target.xyz);

    direction = view.invV * vec4(rayDirEyeSpace, 0.0f);
}

#endif // CRISP_PATH_TRACER_PERSPECTIVE_GLSL
