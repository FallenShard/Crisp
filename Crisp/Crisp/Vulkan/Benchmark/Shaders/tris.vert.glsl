#version 460 core

layout(push_constant) uniform Params {
    vec2 viewportSize;
    float legPixels;
    uint trianglesPerRow;
}
params;

void main() {
    const uint triangleIndex = uint(gl_VertexIndex) / 3u;
    const uint corner = uint(gl_VertexIndex) % 3u;

    const uint column = triangleIndex % params.trianglesPerRow;
    const uint row = triangleIndex / params.trianglesPerRow;

    const float rowsPerScreen = max(floor(params.viewportSize.y / params.legPixels), 1.0f);
    const vec2 origin =
        vec2(float(column) * params.legPixels, mod(float(row), rowsPerScreen) * params.legPixels);

    vec2 offset = vec2(0.0f);
    if (corner == 1u) {
        offset = vec2(params.legPixels, 0.0f);
    } else if (corner == 2u) {
        offset = vec2(0.0f, params.legPixels);
    }

    const vec2 pixelPosition = origin + offset;
    const vec2 ndc = (pixelPosition / params.viewportSize) * 2.0f - 1.0f;
    gl_Position = vec4(ndc, 0.0f, 1.0f);
}
