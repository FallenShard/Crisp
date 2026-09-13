#ifndef CRISP_MIRROR_GLSL
#define CRISP_MIRROR_GLSL

void sampleMirror(const vec3 wi, out vec3 wo, out vec3 f, out float pdf) {
    wo = vec3(-wi.xy, wi.z);
    f = vec3(1.0f);
    pdf = 1.0f;
}

#endif // CRISP_MIRROR_GLSL
