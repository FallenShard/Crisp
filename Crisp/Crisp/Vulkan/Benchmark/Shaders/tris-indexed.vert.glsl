#version 460 core

layout(push_constant) uniform Params {
    vec2 viewportSize;
    float legPixels;
    uint verticesPerRow;
}
params;

void main() {
    const uint column = uint(gl_VertexIndex) % params.verticesPerRow;
    const uint row = uint(gl_VertexIndex) / params.verticesPerRow;

    const vec2 pixelPosition = vec2(float(column), float(row)) * params.legPixels;
    const vec2 ndc = (pixelPosition / params.viewportSize) * 2.0f - 1.0f;
    gl_Position = vec4(ndc, 0.0f, 1.0f);
}
