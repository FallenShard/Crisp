#ifndef CRISP_VIEW_PART_GLSL
#define CRISP_VIEW_PART_GLSL

struct ViewParameters {
    mat4 V;
    mat4 P;
    mat4 invV;
    mat4 invP;
    vec2 screenSize;
    vec2 nearFar;
};

#endif // CRISP_VIEW_PART_GLSL
